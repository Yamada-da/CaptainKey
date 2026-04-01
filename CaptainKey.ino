#ifndef USER_USB_RAM
#error "This example needs to be compiled with a USER USB setting"
#endif

#include "src/userUsbHidKeyboard/USBCDC.h"
#include "src/userUsbHidKeyboard/USBHIDKeyboard.h"
#include "src/userUsbHidKeyboard/WS2812.h"

#define VERSION 0x01       // 版本号 范围0x00-0xFE 修改此值可以使默认配置覆盖EEPROM
#define PRODUCT_TYPE 0x00  // 产品类型 范围0x00-0xFF

#define RGB_PIN 14
#define BUTTON_PIN 15

#define NUM_LEDS 1
#define COLOR_PER_LEDS 3
#define NUM_BYTES (NUM_LEDS * COLOR_PER_LEDS)

// 串口接收状态
enum State {
    IDLE,
    WAITING_OPEN_BRACKET,
    READING_KEY,
    WAITING_COLON,
    READING_VALUE,
    WAITING_CLOSE_BRACKET
};

typedef struct HSV {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} CHSV;

typedef struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} CRGB;

// 按下次数
__xdata uint16_t clickCount = 0;

__xdata uint8_t ledData[NUM_BYTES];

// config 索引
enum {
    CONFIG_INDEX_VERSION = 0,                  // 版本号
    CONFIG_INDEX_PRODUCT_TYPE,                 // 产品类型
    CONFIG_INDEX_CLICK_COUNT_0,                // 按下次数
    CONFIG_INDEX_CLICK_COUNT_1,                // 按下次数
    CONFIG_INDEX_KEY_SELECT,                   // 按键选择 (对应值为 2 时开启双击功能, 双击使用 key2 的配置)
    CONFIG_INDEX_RGB_R = 10,                   // RGB 红
    CONFIG_INDEX_RGB_G,                        // RGB 绿
    CONFIG_INDEX_RGB_B,                        // RGB 蓝
    CONFIG_INDEX_RGB_BRIGHTNESS,               // RGB 亮度
    CONFIG_INDEX_RGB_MODE,                     // RGB 模式
    CONFIG_INDEX_RGB_INTERVAL,                 // RGB 间隔
    CONFIG_INDEX_RGB_STEP,                     // RGB 步长
    CONFIG_INDEX_KEY1_MODE = 17,               // KEY1 模式
    CONFIG_INDEX_KEY1_COMBINATION_START = 18,  // KEY1 组合键值开始
    CONFIG_INDEX_KEY1_COMBINATION_END = 22,    // KEY1 组合键值结束
    CONFIG_INDEX_KEY1_TEXT_START = 23,         // KEY1 输入文本开始
    CONFIG_INDEX_KEY1_TEXT_END = 52,           // KEY1 输入文本结束
    CONFIG_INDEX_KEY2_MODE = 53,               // KEY2 模式
    CONFIG_INDEX_KEY2_COMBINATION_START = 54,  // KEY2 组合键值开始
    CONFIG_INDEX_KEY2_COMBINATION_END = 58,    // KEY2 组合键值结束
    CONFIG_INDEX_KEY2_TEXT_START = 59,         // KEY2 输入文本开始
    CONFIG_INDEX_KEY2_TEXT_END = 88,           // KEY2 输入文本结束
    CONFIG_INDEX_LAST,                         // 结束占位
};

__xdata uint8_t config[CONFIG_INDEX_LAST];

// 函数声明
void handle_single_click();
void handle_double_click();
void handle_key_state(bool key_state, bool is_double_click);
void update_value(uint8_t* value, uint8_t min_value, uint8_t max_value, uint8_t step, bool* increasing);
void handle_color_effect();
void update_color_by_rgb(CRGB rgb, uint8_t brightness);
void update_color_by_hsv(CHSV hsv, uint8_t brightness);
void eeprom_write(uint8_t addr, uint8_t value);
void config2eeprom();
void eeprom2config();
void handle_serial_input();
void handle_serial_input_kv(uint16_t key, uint16_t value);
void hsv2rgb(const CHSV* hsv, CRGB* rgb);
void reset_default_config();

void setup() {
    USBInit();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RGB_PIN, OUTPUT);

    // 判断 EEPROM 中的版本号，如果和当前代码版本不一致，则将默认配置写入 EEPROM
    if (eeprom_read_byte(CONFIG_INDEX_VERSION) != VERSION) {
        reset_default_config();
        config2eeprom();
    }
    // 从 EEPROM 中读取配置
    eeprom2config();

    // 从 config 中初始化 clickCount
    clickCount = config[CONFIG_INDEX_CLICK_COUNT_0] << 8 | config[CONFIG_INDEX_CLICK_COUNT_1];

    // 初始化 RGB
    set_pixel_for_GRB_LED(ledData, 0, 0, 0, 0);
    neopixel_show_P1_4(ledData, NUM_BYTES);

    // 等待按键松开
    while (!digitalRead(BUTTON_PIN))
        ;
}

void loop() {
    // 灯效
    handle_color_effect();
    // 处理串口输入
    handle_serial_input();

    // 按键处理
    if (config[CONFIG_INDEX_KEY_SELECT] == 2) {
        handle_double_click();
    } else {
        handle_single_click();
    }

    // 定时保存 clickCount, 避免 EEPROM 写入太频繁（注释此代码可让其数值仅保存在RAM不写入EEPROM）
    static unsigned long lastSaveClickCountTime = 0;
    if (millis() - lastSaveClickCountTime > 120000) {
        lastSaveClickCountTime = millis();
        if (clickCount != config[CONFIG_INDEX_CLICK_COUNT_0] << 8 | config[CONFIG_INDEX_CLICK_COUNT_1]) {
            config[CONFIG_INDEX_CLICK_COUNT_0] = clickCount >> 8;
            config[CONFIG_INDEX_CLICK_COUNT_1] = clickCount;

            eeprom_write(CONFIG_INDEX_CLICK_COUNT_0, config[CONFIG_INDEX_CLICK_COUNT_0]);
            eeprom_write(CONFIG_INDEX_CLICK_COUNT_1, config[CONFIG_INDEX_CLICK_COUNT_1]);
        }
    }
}

void handle_single_click() {
    static bool buttonPressPrev = false;
    bool buttonPress = !digitalRead(BUTTON_PIN);
    unsigned long currentTime = millis();
    static unsigned long debounceTime = 0;

    if (buttonPress != buttonPressPrev && currentTime - debounceTime > 10) {
        debounceTime = currentTime;
        buttonPressPrev = buttonPress;
        handle_key_state(buttonPress, false);  // 按键处理函数
    }
}

void handle_double_click() {
    // 使用第一次按下与第二次按下的时间差区分单击和双击
    // 第一次按下时，开始 {timeDiff}ms 延迟触发单击按下和释放事件，
    // 如果期间内再次按下则取消计时器，马上触发双击按下事件，然后抬起时触发双击释放事件
    // 优化使用体验, 动态调节 timeDiff 的值, 如果上次为单击则 timeDiffMin，如果上次为双击则 timeDiffMax
    static uint16_t timeDiff = 200;
    const uint16_t timeDiffMin = 200;
    const uint16_t timeDiffMax = 350;

    // 单击按下事件定时
    static unsigned long startTimer1 = 0;
    // 双击释放事件定时
    static unsigned long startTimer2 = 0;

    static bool buttonPressPrev = false;
    bool buttonPress = !digitalRead(BUTTON_PIN);
    unsigned long currentTime = millis();
    static unsigned long debounceTime = 0;

    if (buttonPress != buttonPressPrev && currentTime - debounceTime > 10) {
        buttonPressPrev = buttonPress;
        debounceTime = currentTime;

        if (buttonPress) {
            if (startTimer1 == 0 && startTimer2 == 0) {
                // 这时说明按钮第一次按下
                // 开始延迟触发单击按下和释放事件
                startTimer1 = currentTime + timeDiff;
                startTimer2 = currentTime + timeDiff;
            } else if (startTimer1 != 0 && startTimer2 != 0) {
                // 这时说明按钮第二次按下, 并且单击按下和释放事件都未触发
                // 取消单击定时器，马上触发双击按下事件
                startTimer1 = 0;
                startTimer2 = 0;
                // 在此处理双击按下事件
                handle_key_state(true, true);
            }
        } else {
            if (startTimer2 != 0) {
                // 这时说明单击按下事件已经触发，单击释放事件还没触发
                // 使它能够在下面的定时器判断中立即触发单击释放事件
                startTimer2 = currentTime;
            } else {
                // 在此处理双击释放事件
                handle_key_state(false, true);
                timeDiff = timeDiffMax;
            }
        }
    } else {
        if (buttonPress && startTimer2 != 0) {
            // 这时说明在一直按住的状态，并且还没触发单击释放定时器
            // 把释放事件定时器推迟, 防止本次loop内触发单击释放事件
            startTimer2 = currentTime + 1;
        }
    }

    // 单击按下计时器
    if (startTimer1 != 0 && currentTime >= startTimer1) {
        startTimer1 = 0;
        // 在此处理单击按下事件
        handle_key_state(true, false);
    }
    // 单击释放计时器
    if (startTimer1 == 0 && startTimer2 != 0 && currentTime >= startTimer2) {
        startTimer2 = 0;
        // 在此处理单击释放事件
        handle_key_state(false, false);
        timeDiff = timeDiffMin;
    }
}

void handle_key_state(bool key_state, bool is_double_click) {
    static int8_t endIndex = -1;

    const uint8_t config_key_mode = config[is_double_click ? CONFIG_INDEX_KEY2_MODE : CONFIG_INDEX_KEY1_MODE];
    const uint8_t config_index_key_combination_start = is_double_click ? CONFIG_INDEX_KEY2_COMBINATION_START : CONFIG_INDEX_KEY1_COMBINATION_START;
    const uint8_t config_index_key_combination_end = is_double_click ? CONFIG_INDEX_KEY2_COMBINATION_END : CONFIG_INDEX_KEY1_COMBINATION_END;
    const uint8_t config_index_key_text_start = is_double_click ? CONFIG_INDEX_KEY2_TEXT_START : CONFIG_INDEX_KEY1_TEXT_START;
    const uint8_t config_index_key_text_end = is_double_click ? CONFIG_INDEX_KEY2_TEXT_END : CONFIG_INDEX_KEY1_TEXT_END;

    // 释放
    if (!key_state) {
        // 释放时更新一下 clickCount
        clickCount++;
        switch (config_key_mode) {
            case 0:  // 单键模式0 松开后释放
                Keyboard_release(config[config_index_key_combination_start + 0]);
                break;
            case 2:  // 组合键模式2 松开后 倒序释放
                for (uint8_t i = endIndex; i >= config_index_key_combination_start; i--) {
                    Keyboard_release(config[i]);
                }
                endIndex = -1;
                break;
        }
        // 防止遗漏，释放所有
        Keyboard_releaseAll();
        return;
    }

    // 按下
    switch (config_key_mode) {
        case 0:  // 单键模式
            Keyboard_press(config[config_index_key_combination_start + 0]);
            break;
        case 1:  // 组合键模式1 顺序按下接着倒序释放
        case 2:  // 组合键模式2 顺序按下 等待松开后 倒序释放
            // 模式1 和 模式2 都先顺序按下
            for (uint8_t i = config_index_key_combination_start; i <= config_index_key_combination_end; i++) {
                // 轮流按下直到 0xFF 或 0x00 结束
                if (config[i] == 0xFF || config[i] == 0x00) {
                    break;
                }
                Keyboard_press(config[i]);
                endIndex = i;
            }
            // 如果是模式1 接着从 endIndex 开始倒序释放
            if (config_key_mode == 1) {
                for (uint8_t i = endIndex; i >= config_index_key_combination_start; i--) {
                    Keyboard_release(config[i]);
                }
                endIndex = -1;
            }
            break;
        case 3:  // 输入文本模式
            for (uint8_t i = config_index_key_text_start; i <= config_index_key_text_end; i++) {
                if (config[i] == 0xFF || config[i] == 0x00) {
                    break;
                }
                Keyboard_write(config[i]);
            }
            break;
    }
}

void update_value(uint8_t* value, uint8_t min_value, uint8_t max_value, uint8_t step, bool* increasing) {
    int16_t _value = *value;
    if (*increasing) {
        _value += step;
        if (_value >= max_value) {
            _value = max_value;
            *increasing = false;
        }
    } else {
        _value -= step;
        if (_value <= min_value) {
            _value = min_value;
            *increasing = true;
        }
    }
    *value = _value;
}

void handle_color_effect() {
    static unsigned long lastBreathTime = 0;
    static bool increasing = true;                // 方向标志，true 表示递增，false 表示递减
    static uint8_t currentBrightness = 0;         // 仅在亮度呼吸效果中使用
    static CHSV currentHSVColor = {0, 255, 255};  // 仅在 hue 渐变效果中使用
    if (millis() - lastBreathTime > (config[CONFIG_INDEX_RGB_MODE] == 0 ? 1000 : config[CONFIG_INDEX_RGB_INTERVAL])) {
        lastBreathTime = millis();
        CRGB rgb = {config[CONFIG_INDEX_RGB_R], config[CONFIG_INDEX_RGB_G], config[CONFIG_INDEX_RGB_B]};
        switch (config[CONFIG_INDEX_RGB_MODE]) {
            case 0:
                update_color_by_rgb(rgb, config[CONFIG_INDEX_RGB_BRIGHTNESS]);
                break;
            case 1:
                update_color_by_rgb(rgb, currentBrightness);
                update_value(&currentBrightness, 0, config[CONFIG_INDEX_RGB_BRIGHTNESS], config[CONFIG_INDEX_RGB_STEP], &increasing);
                break;
            case 2:
                currentHSVColor.v = config[CONFIG_INDEX_RGB_BRIGHTNESS];  // hsv，用 v 限制亮度
                update_color_by_hsv(currentHSVColor, 255);
                update_value(&currentHSVColor.h, 0, 255, config[CONFIG_INDEX_RGB_STEP], &increasing);
                break;
        }
    }
}

void update_color_by_rgb(CRGB rgb, uint8_t brightness) {
    static CRGB lastRgb = {0, 0, 0};
    static uint8_t lastBrightness = 0;
    if (rgb.r == lastRgb.r && rgb.g == lastRgb.g && rgb.b == lastRgb.b && brightness == lastBrightness) return;
    lastRgb = rgb;
    lastBrightness = brightness;
    set_pixel_for_GRB_LED(ledData, 0, rgb.r * brightness / 255, rgb.g * brightness / 255, rgb.b * brightness / 255);
    neopixel_show_P1_4(ledData, NUM_BYTES);
}

void update_color_by_hsv(CHSV hsv, uint8_t brightness) {
    CRGB rgb = {0, 0, 0};
    hsv2rgb(&hsv, &rgb);
    update_color_by_rgb(rgb, brightness);
}

void eeprom_write(uint8_t addr, uint8_t value) {
    if (eeprom_read_byte(addr) == value) return;
    eeprom_write_byte(addr, value);

    // USBSerial_print_s("EEPROM w");
    // USBSerial_println(addr);
    // USBSerial_flush();
}

void config2eeprom() {
    for (uint8_t i = 0; i < CONFIG_INDEX_LAST; i++) {
        eeprom_write(i, config[i]);
    }
}

void eeprom2config() {
    for (uint8_t i = 0; i < CONFIG_INDEX_LAST; i++) {
        config[i] = eeprom_read_byte(i);
    }
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

void handle_serial_input() {
    static enum State currentState = IDLE;
    static uint16_t key = 0;
    static uint16_t value = 0;

    while (USBSerial_available()) {
        char serialChar = USBSerial_read();

        switch (currentState) {
            case IDLE:
                if (serialChar == '{') {
                    currentState = WAITING_OPEN_BRACKET;
                }
                break;

            case WAITING_OPEN_BRACKET:
                if (is_digit(serialChar)) {
                    key = serialChar - '0';
                    currentState = READING_KEY;
                }
                break;

            case READING_KEY:
                if (is_digit(serialChar)) {
                    key = key * 10 + (serialChar - '0');
                } else if (serialChar == ':') {
                    currentState = WAITING_COLON;
                }
                break;

            case WAITING_COLON:
                if (is_digit(serialChar)) {
                    value = serialChar - '0';
                    currentState = READING_VALUE;
                }
                break;

            case READING_VALUE:
                if (is_digit(serialChar)) {
                    value = value * 10 + (serialChar - '0');
                } else if (serialChar == '}') {
                    currentState = IDLE;
                    // 处理接收完整的消息
                    handle_serial_input_kv(key, value);
                }
                break;
        }
    }
}

void handle_serial_input_kv(uint16_t key, uint16_t value) {
    // 写入配置
    if (key < CONFIG_INDEX_LAST && value <= 255) {
        if (key == CONFIG_INDEX_VERSION || key == CONFIG_INDEX_PRODUCT_TYPE) return;
        config[key] = value;
        return;
    }
    // 命令
    if (key == 999) {
        switch (value) {
            case 0:  // 保存 config 到 eeprom
                config2eeprom();
                break;
            case 1:  // 打印 config 数组
                for (uint8_t i = 0; i < CONFIG_INDEX_LAST; i++) {
                    USBSerial_print_c(i == 0 ? '[' : ',');
                    // 打印按下次数时, 使用 clickCount 的值替换, 确保可以实时更新数值
                    if (i == CONFIG_INDEX_CLICK_COUNT_0) {
                        USBSerial_print(clickCount >> 8);
                    } else if (i == CONFIG_INDEX_CLICK_COUNT_1) {
                        USBSerial_print(clickCount & 0xFF);
                    } else {
                        USBSerial_print(config[i]);
                    }
                }
                USBSerial_print_c(']');
                USBSerial_flush();
                break;
            case 2:  // 立即更新 RGB
                if (config[CONFIG_INDEX_RGB_MODE] == 0) {
                    CRGB rgb = {config[CONFIG_INDEX_RGB_R], config[CONFIG_INDEX_RGB_G], config[CONFIG_INDEX_RGB_B]};
                    update_color_by_rgb(rgb, config[CONFIG_INDEX_RGB_BRIGHTNESS]);
                }
                break;
        }
    }
}

void hsv2rgb(const CHSV* hsv, CRGB* rgb) {
    uint8_t region, p, q, t;
    uint16_t h, s, v, remainder;

    if (hsv->s == 0) {
        rgb->r = rgb->g = rgb->b = hsv->v;
        return;
    }

    h = hsv->h;
    s = hsv->s;
    v = hsv->v;

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            rgb->r = v;
            rgb->g = t;
            rgb->b = p;
            break;
        case 1:
            rgb->r = q;
            rgb->g = v;
            rgb->b = p;
            break;
        case 2:
            rgb->r = p;
            rgb->g = v;
            rgb->b = t;
            break;
        case 3:
            rgb->r = p;
            rgb->g = q;
            rgb->b = v;
            break;
        case 4:
            rgb->r = t;
            rgb->g = p;
            rgb->b = v;
            break;
        default:
            rgb->r = v;
            rgb->g = p;
            rgb->b = q;
            break;
    }
}

void reset_default_config() {
    // 版本号
    config[CONFIG_INDEX_VERSION] = VERSION;
    // 产品类型
    config[CONFIG_INDEX_PRODUCT_TYPE] = PRODUCT_TYPE;
    // 按下次数, 范围 0-65535
    config[CONFIG_INDEX_CLICK_COUNT_0] = 0x00;
    config[CONFIG_INDEX_CLICK_COUNT_1] = 0x00;
    // 按键选择
    config[CONFIG_INDEX_KEY_SELECT] = 2;
    // RGB
    config[CONFIG_INDEX_RGB_R] = config[CONFIG_INDEX_RGB_G] = config[CONFIG_INDEX_RGB_B] = config[CONFIG_INDEX_RGB_BRIGHTNESS] = 255;
    config[CONFIG_INDEX_RGB_MODE] = 2;
    config[CONFIG_INDEX_RGB_INTERVAL] = 30;
    config[CONFIG_INDEX_RGB_STEP] = 1;
    // 默认按键
	// 1. 设置默认按键模式 (模式0：单键；模式1：按下后立即释放；模式 2: 按下后手指离开时释放；模式3：文本)
    config[CONFIG_INDEX_KEY1_MODE] = 2; // KEY1 (单击) 设置为组合键模式 [cite: 125]
    config[CONFIG_INDEX_KEY2_MODE] = 2; // KEY2 (双击) 设置为组合键模式 [cite: 125]
    // 2. 配置 KEY1 (单击) 为 Mute + Windows + D
	config[CONFIG_INDEX_KEY1_COMBINATION_START + 0] = 232;          // 静音键值232
    config[CONFIG_INDEX_KEY1_COMBINATION_START + 1] = KEY_LEFT_GUI; // Windows 键 [cite: 126]
    config[CONFIG_INDEX_KEY1_COMBINATION_START + 2] = 'd';          // D 键 [cite: 126]
    // 3. 配置 KEY2 (双击) 为 Windows + L
    config[CONFIG_INDEX_KEY2_COMBINATION_START + 0] = KEY_LEFT_GUI; // Windows 键 [cite: 126]
    config[CONFIG_INDEX_KEY2_COMBINATION_START + 1] = 'l';          // L 键 [cite: 126]
	//默认文本设置 (KEY1)
    config[CONFIG_INDEX_KEY1_TEXT_START + 0] = 'S';
    config[CONFIG_INDEX_KEY1_TEXT_START + 1] = 'a';
    config[CONFIG_INDEX_KEY1_TEXT_START + 2] = 'v';
    config[CONFIG_INDEX_KEY1_TEXT_START + 3] = 'e';
    config[CONFIG_INDEX_KEY1_TEXT_START + 4] = ' ';
    config[CONFIG_INDEX_KEY1_TEXT_START + 5] = 'U';
    config[CONFIG_INDEX_KEY1_TEXT_START + 6] = ' ';
    config[CONFIG_INDEX_KEY1_TEXT_START + 7] = 'P';
    config[CONFIG_INDEX_KEY1_TEXT_START + 8] = 'l';
	config[CONFIG_INDEX_KEY1_TEXT_START + 9] = 'a';
    config[CONFIG_INDEX_KEY1_TEXT_START + 10] = 'n';
    config[CONFIG_INDEX_KEY1_TEXT_START + 11] = 'e';
    //默认文本设置 (KEY2)
    config[CONFIG_INDEX_KEY2_TEXT_START + 0] = 'N';
    config[CONFIG_INDEX_KEY2_TEXT_START + 1] = 'o';
    config[CONFIG_INDEX_KEY2_TEXT_START + 2] = ' ';
    config[CONFIG_INDEX_KEY2_TEXT_START + 3] = 'M';
    config[CONFIG_INDEX_KEY2_TEXT_START + 4] = 'o';
    config[CONFIG_INDEX_KEY2_TEXT_START + 5] = 'r';
    config[CONFIG_INDEX_KEY2_TEXT_START + 6] = 'e';
    config[CONFIG_INDEX_KEY2_TEXT_START + 7] = ' ';
    config[CONFIG_INDEX_KEY2_TEXT_START + 8] = 'C';
    config[CONFIG_INDEX_KEY2_TEXT_START + 9] = 'r';
    config[CONFIG_INDEX_KEY2_TEXT_START + 10] = 'a';
    config[CONFIG_INDEX_KEY2_TEXT_START + 11] = 's';
    config[CONFIG_INDEX_KEY2_TEXT_START + 12] = 'h';
}
