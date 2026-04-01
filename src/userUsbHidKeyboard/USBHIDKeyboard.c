#include <stdint.h>
#include <stdbool.h>
#include "include/ch5xx.h"
#include "include/ch5xx_usb.h"
#include "USBconstant.h"
#include "USBhandler.h"
#include "USBHIDKeyboard.h"

volatile __xdata uint8_t UpPoint3_Busy  = 0;   //Flag of whether upload pointer is busy

__xdata uint8_t HIDKey[9] = {0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

void Consumer_press(uint16_t key);
void Consumer_release(void);

#define SHIFT 0x80
__code uint8_t _asciimap[128] =
{
	0x00,             // NUL
	0x00,             // SOH
	0x00,             // STX
	0x00,             // ETX
	0x00,             // EOT
	0x00,             // ENQ
	0x00,             // ACK  
	0x00,             // BEL
	0x2a,			// BS	Backspace
	0x2b,			// TAB	Tab
	0x28,			// LF	Enter
	0x00,             // VT 
	0x00,             // FF 
	0x00,             // CR 
	0x00,             // SO 
	0x00,             // SI 
	0x00,             // DEL
	0x00,             // DC1
	0x00,             // DC2
	0x00,             // DC3
	0x00,             // DC4
	0x00,             // NAK
	0x00,             // SYN
	0x00,             // ETB
	0x00,             // CAN
	0x00,             // EM 
	0x00,             // SUB
	0x00,             // ESC
	0x00,             // FS 
	0x00,             // GS 
	0x00,             // RS 
	0x00,             // US 

	0x2c,		   //  ' '
	0x1e|SHIFT,	   // !
	0x34|SHIFT,	   // "
	0x20|SHIFT,    // #
	0x21|SHIFT,    // $
	0x22|SHIFT,    // %
	0x24|SHIFT,    // &
	0x34,          // '
	0x26|SHIFT,    // (
	0x27|SHIFT,    // )
	0x25|SHIFT,    // *
	0x2e|SHIFT,    // +
	0x36,          // ,
	0x2d,          // -
	0x37,          // .
	0x38,          // /
	0x27,          // 0
	0x1e,          // 1
	0x1f,          // 2
	0x20,          // 3
	0x21,          // 4
	0x22,          // 5
	0x23,          // 6
	0x24,          // 7
	0x25,          // 8
	0x26,          // 9
	0x33|SHIFT,      // :
	0x33,          // ;
	0x36|SHIFT,      // <
	0x2e,          // =
	0x37|SHIFT,      // >
	0x38|SHIFT,      // ?
	0x1f|SHIFT,      // @
	0x04|SHIFT,      // A
	0x05|SHIFT,      // B
	0x06|SHIFT,      // C
	0x07|SHIFT,      // D
	0x08|SHIFT,      // E
	0x09|SHIFT,      // F
	0x0a|SHIFT,      // G
	0x0b|SHIFT,      // H
	0x0c|SHIFT,      // I
	0x0d|SHIFT,      // J
	0x0e|SHIFT,      // K
	0x0f|SHIFT,      // L
	0x10|SHIFT,      // M
	0x11|SHIFT,      // N
	0x12|SHIFT,      // O
	0x13|SHIFT,      // P
	0x14|SHIFT,      // Q
	0x15|SHIFT,      // R
	0x16|SHIFT,      // S
	0x17|SHIFT,      // T
	0x18|SHIFT,      // U
	0x19|SHIFT,      // V
	0x1a|SHIFT,      // W
	0x1b|SHIFT,      // X
	0x1c|SHIFT,      // Y
	0x1d|SHIFT,      // Z
	0x2f,          // [
	0x31,          // bslash
	0x30,          // ]
	0x23|SHIFT,    // ^
	0x2d|SHIFT,    // _
	0x35,          // `
	0x04,          // a
	0x05,          // b
	0x06,          // c
	0x07,          // d
	0x08,          // e
	0x09,          // f
	0x0a,          // g
	0x0b,          // h
	0x0c,          // i
	0x0d,          // j
	0x0e,          // k
	0x0f,          // l
	0x10,          // m
	0x11,          // n
	0x12,          // o
	0x13,          // p
	0x14,          // q
	0x15,          // r
	0x16,          // s
	0x17,          // t
	0x18,          // u
	0x19,          // v
	0x1a,          // w
	0x1b,          // x
	0x1c,          // y
	0x1d,          // z
	0x2f|SHIFT,    // {
	0x31|SHIFT,    // |
	0x30|SHIFT,    // }
	0x35|SHIFT,    // ~
	0				// DEL
};

typedef void( *pTaskFn)( void );

void delayMicroseconds(uint16_t us);

void USB_EP3_IN(){
    UEP3_T_LEN = 0;
    UEP3_CTRL = UEP3_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK;           // Default NAK
    UpPoint3_Busy = 0;                                                  //Clear busy flag
}


uint8_t USB_EP3_send(){
    uint16_t waitWriteCount = 0;
    
    waitWriteCount = 0;
    while (UpPoint3_Busy){//wait for 250ms or give up
        waitWriteCount++;
        delayMicroseconds(5);   
        if (waitWriteCount>=50000) return 0;
    }
    
    for (uint8_t i=0;i<sizeof(HIDKey);i++){                                  //load data for upload
        Ep3Buffer[64+i] = HIDKey[i];
    }
                   
    UEP3_T_LEN = sizeof(HIDKey);                                             //data length
    UpPoint3_Busy = 1;
    UEP3_CTRL = UEP3_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_ACK;                //upload data and respond ACK
    
    return 1;
}


void USB_Keyboard_SendKey(uint8_t modifier, uint8_t key) {
    HIDKey[1] = modifier;
    HIDKey[2] = 0;        
    HIDKey[3] = key;      
    HIDKey[4] = 0;        
    HIDKey[5] = 0;        
    HIDKey[6] = 0;        
    HIDKey[7] = 0;        
    HIDKey[8] = 0;        
    USB_EP3_send();
}

uint8_t Keyboard_press(uint8_t k) {
    // 拦截并处理多媒体键
    if (k >= 230 && k <= 235) {
        uint16_t media_usage = 0;
        switch(k) {
            case 230: media_usage = 0x00E9; break; // Vol+
            case 231: media_usage = 0x00EA; break; // Vol-
            case 232: media_usage = 0x00E2; break; // Mute
            case 233: media_usage = 0x00CD; break; // Play/Pause
            case 234: media_usage = 0x00B6; break; // Prev
            case 235: media_usage = 0x00B5; break; // Next
        }
        Consumer_press(media_usage);
        return 1;
    }

    uint8_t i;
    if (k >= 136 && k < 230) {        
        k = k - 136;
    } else if (k >= 128 && k < 136) { 
        HIDKey[1] |= (1<<(k-128));
        k = 0;
    } else {                          
        k = _asciimap[k];
        if (!k) return 0;
        if (k & 0x80) {               
            HIDKey[1] |= 0x02;        
            k &= 0x7F;
        }
    }
    
    if (HIDKey[3] != k && HIDKey[4] != k && HIDKey[5] != k && 
        HIDKey[6] != k && HIDKey[7] != k && HIDKey[8] != k) {
        for (i=3; i<9; i++) {
            if (HIDKey[i] == 0x00) {
                HIDKey[i] = k;
                break;
            }
        }
        if (i == 9) return 0;    
    }
    USB_EP3_send();
    return 1;
}

uint8_t Keyboard_release(uint8_t k) {
    // 拦截多媒体键释放
    if (k >= 230 && k <= 235) {
        Consumer_release();
        return 1;
    }

    uint8_t i;
    if (k >= 136 && k < 230) {        
        k = k - 136;
    } else if (k >= 128 && k < 136) { 
        HIDKey[1] &= ~(1<<(k-128));
        k = 0;
    } else {                          
        k = _asciimap[k];
        if (!k) return 0;
        if (k & 0x80) {
            HIDKey[1] &= ~(0x02);
            k &= 0x7F;
        }
    }
    
    for (i=3; i<9; i++) {
        if (0 != k && HIDKey[i] == k) {
            HIDKey[i] = 0x00;
        }
    }
    USB_EP3_send();
    return 1;
}

void Keyboard_releaseAll(void){
    for (uint8_t i=1; i<sizeof(HIDKey); i++){
        HIDKey[i] = 0;
    }
    USB_EP3_send();
    Consumer_release(); // 释放按键时确保多媒体键也一块释放
}

uint8_t Keyboard_write(uint8_t c){
	uint8_t p = Keyboard_press(c);  // Keydown
	Keyboard_release(c);            // Keyup
	return p;              // just return the result of press() since release() almost always returns 1
}

uint8_t Keyboard_getLEDStatus(){
    return Ep3Buffer[1];  // LED状态字
}

void Consumer_press(uint16_t key) {
    // 1. 等待通道空闲
    while(UpPoint3_Busy);
    
    // 2. 组装多媒体数据包，必须写在 64 及以上的偏移地址（发送区）！
    Ep3Buffer[64 + 0] = 0x02;               // Report ID 2
    Ep3Buffer[64 + 1] = key & 0xFF;         // 按键码低8位
    Ep3Buffer[64 + 2] = (key >> 8) & 0xFF;  // 按键码高8位
    
    // 3. 扣动扳机发送 3 个字节
    UEP3_T_LEN = 3;                    
    UpPoint3_Busy = 1;                 
    UEP3_CTRL = UEP3_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK; 
}

void Consumer_release(void) {
    while(UpPoint3_Busy);
    
    // 释放多媒体按键
    Ep3Buffer[64 + 0] = 0x02; 
    Ep3Buffer[64 + 1] = 0x00;
    Ep3Buffer[64 + 2] = 0x00;
    
    UEP3_T_LEN = 3; 
    UpPoint3_Busy = 1;
    UEP3_CTRL = UEP3_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK; 
}