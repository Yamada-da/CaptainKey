import os
import sys
import serial
import serial.tools.list_ports
import time
import tkinter as tk
from tkinter import ttk, messagebox
import ctypes

# --- 开启 Windows 高 DPI 缩放感知 ---
try:
    # 适用于 Windows 8.1 及以上
    ctypes.windll.shcore.SetProcessDpiAwareness(1)
except Exception:
    try:
        # 适用于 Windows Vista / 8
        ctypes.windll.user32.SetProcessDPIAware()
    except Exception:
        pass

# --- 按键字典映射 ---
KEY_MAP = {
    "无 (None)": 0,
    "左 Ctrl": 128, "左 Shift": 129, "左 Alt": 130, "左 Win": 131,
    "右 Ctrl": 132, "右 Shift": 133, "右 Alt": 134, "右 Win": 135,
    "Enter": 176, "Esc": 177, "Backspace": 178, "Tab": 179, "Space": 32,
    "Up": 218, "Down": 217, "Left": 216, "Right": 215,
    "Insert": 209, "Delete": 212, "Home": 210, "End": 213, "Page Up": 211, "Page Down": 214,
    "音量+ (Vol+)": 230, "音量- (Vol-)": 231, "静音 (Mute)": 232, 
    "播放/暂停 (Play)": 233, "上一曲 (Prev)": 234, "下一曲 (Next)": 235,
    "A": 97, "B": 98, "C": 99, "D": 100, "E": 101, "F": 102, "G": 103, "H": 104,
    "I": 105, "J": 106, "K": 107, "L": 108, "M": 109, "N": 110, "O": 111, "P": 112,
    "Q": 113, "R": 114, "S": 115, "T": 116, "U": 117, "V": 118, "W": 119, "X": 120,
    "Y": 121, "Z": 122,
    "1": 49, "2": 50, "3": 51, "4": 52, "5": 53, "6": 54, "7": 55, "8": 56, "9": 57, "0": 48
}

def get_key_name_by_val(val):
    for name, v in KEY_MAP.items():
        if v == val: return name
    return "无 (None)" if val == 0 else f"CHR({val})"

def resource_path(relative_path):
    """ 获取资源的绝对路径，兼容开发环境和 PyInstaller 打包环境 """
    if hasattr(sys, '_MEIPASS'):
        return os.path.join(sys._MEIPASS, relative_path)
    return os.path.join(os.path.abspath("."), relative_path)

class CaptainKey:
    def __init__(self, root):
        self.root = root
        self.root.withdraw() # 立即隐藏窗口，避免灰色闪烁
        self.root.title("CaptainKey 配置工具 v0.5")
        self.root.minsize(400, 560) 
        self.serial_port = None
        self._is_reading = False
        try:
            self.root.iconbitmap(resource_path('icon.ico'))
        except Exception:
            pass # 防止图标缺失导致程序无法启动
        # --- 用于彩蛋的点击计数器 ---
        self._info_click_count = 0
        self._info_click_timer = None

        self.build_ui()

        self.root.update_idletasks() # 确保所有组件已经计算好布局
        self.root.deiconify()        # 显示已渲染完成的窗口

    def build_ui(self):
        # --- 顶部：连接区 (分为两行) ---
        top_frame = ttk.Frame(self.root, padding=10)
        top_frame.pack(fill=tk.X)
        
        row1 = ttk.Frame(top_frame)
        row1.pack(fill=tk.X)
        row2 = ttk.Frame(top_frame)
        row2.pack(fill=tk.X, pady=(5, 0))
        
        ttk.Label(row1, text="串口:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(row1, textvariable=self.port_var, state="readonly", width=18)
        self.refresh_ports()
        self.port_combo.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(row1, text="刷新", command=self.refresh_ports, width=6).pack(side=tk.LEFT, padx=2)
        self.btn_connect = ttk.Button(row1, text="连接", command=self.toggle_connection, width=6)
        self.btn_connect.pack(side=tk.LEFT, padx=5)

        self.info_label = ttk.Label(row2, text="未连接 | 点击次数: - | 此版本适配于CaptainKey_v01固件", foreground="gray")
        self.info_label.pack(side=tk.LEFT)

        # --- 绑定左键点击事件用于彩蛋 ---
        self.info_label.bind("<Button-1>", self.on_info_click)

        # --- 中部：选项卡 ---
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.tab_rgb = ttk.Frame(self.notebook, padding=10)
        self.tab_key1 = ttk.Frame(self.notebook, padding=10)
        self.tab_key2 = ttk.Frame(self.notebook, padding=10)

        self.notebook.add(self.tab_rgb, text="🌈 灯效设置")
        self.notebook.add(self.tab_key1, text="⌨ 单击(KEY1)") 
        self.notebook.add(self.tab_key2, text="⌨ 双击(KEY2)")

        self.build_rgb_tab()
        self.build_key_tab(self.tab_key1, prefix="k1", is_double=False)
        self.build_key_tab(self.tab_key2, prefix="k2", is_double=True)

        # --- 底部：保存区 ---
        bottom_frame = ttk.Frame(self.root, padding=10)
        bottom_frame.pack(fill=tk.X)
        ttk.Label(bottom_frame, text="提示: 修改将在设备拔插后失效，如需保存请点击→").pack(side=tk.LEFT)
        ttk.Button(bottom_frame, text="保存至键盘", command=self.save_to_eeprom).pack(side=tk.RIGHT, padx=5)

    # ================= 彩蛋弹窗 =================
    def on_info_click(self, event):
        self._info_click_count += 1
        
        # 如果之前有定时器，取消它（重新计时）
        if self._info_click_timer:
            self.root.after_cancel(self._info_click_timer)
            
        # 设置一个 500 毫秒的定时器，超时则重置计数
        self._info_click_timer = self.root.after(500, self.reset_info_click)
        
        # 如果在 500ms 内连点了 3 次
        if self._info_click_count >= 3:
            self.reset_info_click() # 触发后立即重置
            self.show_about_window()

    def reset_info_click(self):
        self._info_click_count = 0
        self._info_click_timer = None

    def show_about_window(self):
        about_win = tk.Toplevel(self.root)
        about_win.title("关于")
        
        # 修复闪现 创建后立即隐藏窗口
        about_win.withdraw()
        
        # 窗口尺寸
        win_width = 380
        win_height = 450
        about_win.minsize(350, 420)
        
        # 获取主窗口的实时位置和尺寸用于居中
        self.root.update_idletasks()
        root_x = self.root.winfo_x()
        root_y = self.root.winfo_y()
        root_width = self.root.winfo_width()
        root_height = self.root.winfo_height()
        
        pos_x = root_x + (root_width // 2) - (win_width // 2)
        pos_y = root_y + (root_height // 2) - (win_height // 2)
        
        about_win.geometry(f"{win_width}x{win_height}+{pos_x}+{pos_y}")
        
        # 设为模态窗口
        about_win.transient(self.root)
        about_win.grab_set()

        # 主容器
        main_frame = ttk.Frame(about_win, padding=15)
        main_frame.pack(fill=tk.BOTH, expand=True)

        # 先打包底部的关闭按钮
        btn_close = ttk.Button(main_frame, text="关闭", command=about_win.destroy)
        btn_close.pack(side=tk.BOTTOM, pady=(10, 0))

        # 再打包文本框容器
        text_frame = ttk.Frame(main_frame)
        text_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        # 滚动条
        scrollbar = ttk.Scrollbar(text_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # 文本框设置
        text_widget = tk.Text(text_frame, yscrollcommand=scrollbar.set, wrap=tk.WORD, 
                              font=("Microsoft YaHei", 9), bg="#f0f0f0", relief=tk.FLAT, height=10)
        text_widget.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.config(command=text_widget.yview)

        # 设置居中对齐标签
        text_widget.tag_configure("center", justify="center")
        text_widget.tag_configure("title", justify="center", font=("Microsoft YaHei", 11, "bold"))

        # log文本内容
        title_text = "CaptainKey 配置工具\n"
        subtitle_text = "拯救每个绝望机长\n\n"
        changelog = """由CV(Ctrl+C+V)程序猿
Yamada乱写制作
开源项目 付费请举报退款

获取最新版固件及上位机
任何问题请提交issue:
https://github.com/Yamada-da/CaptainKey 

项目源起立创开源项目
https://oshwhub.com/anbin/fish-custom-single-key-keypad-ch552
感谢作者Anbin制作了很棒的项目
通用于原作者开源PCB
此离线版支持配置于原固件(除多媒体键)
刷入CaptianKey固件可额外支持多媒体键

默认单击静音+桌面 双击锁屏
愿天下没有坠机(x

--- 更新日志 ---
[v0.5] | CaptianKey_v1.hex
- 添加图标及程序配套跟进

[v0.4] | CaptianKey_v1.hex
- 增加了多媒体键的支持
请确保刷入了CaptianKey_v1.hex以上固件
- 绘制3D打印外壳

[v0.3]
- 修复界面显示问题

[v0.2]
- 优化界面显示
- 增加高 DPI 适配
- 支持实时的配置应用

[v0.1]
- 立项于Anbin的立创开源项目
https://oshwhub.com/anbin/fish-custom-single-key-keypad-ch552
- 依据作者固件初步制作了单机版的配置工具
"""
        # 插入文本并应用居中样式
        text_widget.insert(tk.END, title_text, "title")
        text_widget.insert(tk.END, subtitle_text, "center")
        text_widget.insert(tk.END, changelog, "left")
        
        # 禁用输入，使其变为只读模式
        text_widget.config(state=tk.DISABLED)

        # 修复闪现 所有UI渲染完成后，重新显示窗口
        about_win.deiconify()

    def refresh_ports(self):
        ports = [f"{p.device} - {p.description}" for p in serial.tools.list_ports.comports()]
        self.port_combo['values'] = ports
        if ports: self.port_combo.current(0)

    def toggle_connection(self):
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()
            self.serial_port = None
            self.btn_connect.config(text="连接")
            self.info_label.config(text="已断开连接", foreground="gray")
            self.port_combo.config(state="readonly")
        else:
            selection = self.port_var.get()
            if not selection: return messagebox.showwarning("警告", "请先选择串口！")
            port = selection.split(" - ")[0]
            try:
                self.serial_port = serial.Serial(port, 57600, timeout=1)
                self.serial_port.reset_input_buffer()
                self.btn_connect.config(text="断开")
                self.port_combo.config(state="disabled")
                self.read_config_from_mcu()
            except Exception as e:
                messagebox.showerror("错误", f"连接失败: {e}")

    # ================= RGB 界面 =================
    def build_rgb_tab(self):
        self.rgb_vars = {
            "mode": tk.IntVar(value=0), "r": tk.IntVar(value=255), "g": tk.IntVar(value=255),
            "b": tk.IntVar(value=255), "bright": tk.IntVar(value=100),
            "step": tk.IntVar(value=1), "interval": tk.IntVar(value=30)
        }
        self.rgb_ui_rows = {}

        row_mode = ttk.Frame(self.tab_rgb)
        row_mode.pack(anchor=tk.W, pady=5)
        ttk.Label(row_mode, text="灯光模式:").pack(side=tk.LEFT, padx=(0, 5))
        modes = [("固定", 0), ("呼吸", 1), ("HUE渐变", 2)]
        for text, val in modes:
            ttk.Radiobutton(row_mode, text=text, variable=self.rgb_vars["mode"], value=val, command=self.update_rgb_ui).pack(side=tk.LEFT, padx=2)

        def create_slider(key, label_text, min_val, max_val):
            frame = ttk.Frame(self.tab_rgb)
            ttk.Label(frame, text=label_text, width=12).pack(side=tk.LEFT)
            
            val_entry = ttk.Entry(frame, textvariable=self.rgb_vars[key], width=5)
            val_entry.pack(side=tk.RIGHT)
            
            # --- 修改点：滑动条事件处理 ---
            def on_scale(v):
                # 将滑动条传来的浮点数强制四舍五入并转为整数，然后再存入变量
                int_val = int(round(float(v)))
                self.rgb_vars[key].set(int_val)
                if not self._is_reading: self.apply_rgb_realtime()

            # --- 输入框手动修改事件处理 ---
            def on_entry_change(*args):
                if not self._is_reading: self.apply_rgb_realtime()

            # 注意这里去掉了原本给 scale 绑定的 variable，改用 command 回调去手动更新整数，彻底消除小数点
            scale = ttk.Scale(frame, from_=min_val, to=max_val, orient=tk.HORIZONTAL, command=on_scale)
            # 为了让滑动条能显示初始值，单独写一个跟随变量的机制
            scale.set(self.rgb_vars[key].get()) 
            scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)
            
            val_entry.bind("<Return>", on_entry_change)
            val_entry.bind("<FocusOut>", on_entry_change)

            self.rgb_ui_rows[key] = frame

        create_slider("r", "红色 (R):", 0, 255)
        create_slider("g", "绿色 (G):", 0, 255)
        create_slider("b", "蓝色 (B):", 0, 255)
        create_slider("bright", "亮度 (V):", 0, 255)
        create_slider("step", "步进 (Step):", 1, 50)
        create_slider("interval", "间隔(ms):", 5, 200)

        self.update_rgb_ui()

    def update_rgb_ui(self):
        mode = self.rgb_vars["mode"].get()
        for frame in self.rgb_ui_rows.values(): frame.pack_forget()
        
        if mode == 0:
            for k in ["r", "g", "b", "bright"]: self.rgb_ui_rows[k].pack(fill=tk.X, pady=5)
        elif mode == 1:
            for k in ["r", "g", "b", "bright", "step", "interval"]: self.rgb_ui_rows[k].pack(fill=tk.X, pady=5)
        elif mode == 2:
            for k in ["bright", "step", "interval"]: self.rgb_ui_rows[k].pack(fill=tk.X, pady=5)
        
        if not self._is_reading: self.apply_rgb_realtime()

    # ================= 按键 界面 =================
    def build_key_tab(self, parent, prefix, is_double):
        setattr(self, f"{prefix}_mode", tk.IntVar(value=0))
        setattr(self, f"{prefix}_combos", [tk.StringVar(value="无 (None)") for _ in range(5)])
        setattr(self, f"{prefix}_text", tk.StringVar(value=""))

        row_mode = ttk.Frame(parent)
        row_mode.pack(anchor=tk.W, pady=5)
        ttk.Label(row_mode, text="触发模式:").pack(side=tk.LEFT, padx=(0, 5))
        
        modes_frame = ttk.Frame(row_mode)
        modes_frame.pack(side=tk.LEFT)
        modes = [("单键", 0), ("顺序组合", 1), ("倒序组合", 2), ("文本输入", 3)]
        
        def on_mode_change():
            self.update_key_ui(prefix)
            if not self._is_reading: self.apply_keys_realtime(prefix, is_double)
            
        for i, (text, val) in enumerate(modes):
            rb = ttk.Radiobutton(modes_frame, text=text, variable=getattr(self, f"{prefix}_mode"), value=val, command=on_mode_change)
            rb.pack(side=tk.LEFT, padx=2)

        ttk.Separator(parent, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=8)

        combo_frame = ttk.Frame(parent)
        setattr(self, f"{prefix}_combo_frame", combo_frame)
        ttk.Label(combo_frame, text="按键设置:").pack(side=tk.LEFT, anchor=tk.N, padx=(0, 5))
        
        box_container = ttk.Frame(combo_frame)
        box_container.pack(side=tk.LEFT)
        
        combo_boxes = []
        def on_combo_change(*args):
            if not self._is_reading: self.apply_keys_realtime(prefix, is_double)

        for i in range(5):
            cb = ttk.Combobox(box_container, textvariable=getattr(self, f"{prefix}_combos")[i], values=list(KEY_MAP.keys()), state="readonly", width=15)
            cb.bind("<<ComboboxSelected>>", on_combo_change)
            cb.pack(side=tk.TOP, pady=2, anchor=tk.W) 
            combo_boxes.append(cb)
        setattr(self, f"{prefix}_combo_boxes", combo_boxes)

        text_frame = ttk.Frame(parent)
        setattr(self, f"{prefix}_text_frame", text_frame)
        ttk.Label(text_frame, text="文本内容\n(限30字):").pack(side=tk.LEFT, anchor=tk.N, padx=(0, 5))
        
        def on_text_change(*args):
            if not self._is_reading: self.apply_keys_realtime(prefix, is_double)
            
        entry = ttk.Entry(text_frame, textvariable=getattr(self, f"{prefix}_text"), width=28)
        entry.pack(side=tk.LEFT, anchor=tk.N)
        entry.bind("<KeyRelease>", on_text_change)

        self.update_key_ui(prefix)

    def update_key_ui(self, prefix):
        mode = getattr(self, f"{prefix}_mode").get()
        combo_frame = getattr(self, f"{prefix}_combo_frame")
        text_frame = getattr(self, f"{prefix}_text_frame")
        boxes = getattr(self, f"{prefix}_combo_boxes")

        combo_frame.pack_forget()
        text_frame.pack_forget()
        for cb in boxes:
            cb.pack_forget()

        if mode == 0:  
            combo_frame.pack(anchor=tk.W, pady=5)
            boxes[0].pack(side=tk.TOP, pady=2, anchor=tk.W)
        elif mode in [1, 2]:  
            combo_frame.pack(anchor=tk.W, pady=5)
            for cb in boxes:
                cb.pack(side=tk.TOP, pady=2, anchor=tk.W)
        elif mode == 3:  
            text_frame.pack(anchor=tk.W, pady=5)

    # ================= 通信逻辑 =================
    def send_cmd(self, index, value):
        if self.serial_port and self.serial_port.is_open:
            cmd = f"{{{index}:{value}}}"
            self.serial_port.write(cmd.encode('ascii'))
            time.sleep(0.015)

    def read_config_from_mcu(self):
        self.send_cmd(999, 1)
        time.sleep(0.2)
        if self.serial_port.in_waiting:
            data = self.serial_port.read(self.serial_port.in_waiting).decode('ascii', errors='ignore')
            
            start, end = data.find('['), data.find(']')
            if start != -1 and end != -1:
                arr = [int(x) for x in data[start+1:end].split(',')]
                if len(arr) < 88: return
                
                self._is_reading = True 
                
                clicks = (arr[2] << 8) | arr[3]
                self.info_label.config(text=f"已连接 | v0{arr[0]} | 按下:{clicks}次 | 此版本适配于CaptainKey_v01固件", foreground="green")

                self.rgb_vars["r"].set(arr[10])
                self.rgb_vars["g"].set(arr[11])
                self.rgb_vars["b"].set(arr[12])
                self.rgb_vars["bright"].set(arr[13])
                self.rgb_vars["mode"].set(arr[14])
                self.rgb_vars["interval"].set(arr[15])
                self.rgb_vars["step"].set(arr[16])
                self.update_rgb_ui() 

                def parse_key(prefix, mode_idx, combo_start, text_start):
                    getattr(self, f"{prefix}_mode").set(arr[mode_idx])
                    for i in range(5):
                        val = arr[combo_start + i]
                        getattr(self, f"{prefix}_combos")[i].set(get_key_name_by_val(val))
                    text_chars = []
                    for i in range(30):
                        val = arr[text_start + i]
                        if val == 0 or val == 255: break
                        text_chars.append(chr(val))
                    getattr(self, f"{prefix}_text").set("".join(text_chars))
                    self.update_key_ui(prefix)

                parse_key("k1", 17, 18, 23)
                parse_key("k2", 53, 54, 59)
                
                self._is_reading = False 

    def apply_rgb_realtime(self):
        try:
            r = self.rgb_vars["r"].get()
            g = self.rgb_vars["g"].get()
            b = self.rgb_vars["b"].get()
            bright = self.rgb_vars["bright"].get()
            mode = self.rgb_vars["mode"].get()
            interval = self.rgb_vars["interval"].get()
            step = self.rgb_vars["step"].get()
        except tk.TclError:
            return

        self.send_cmd(10, r)
        self.send_cmd(11, g)
        self.send_cmd(12, b)
        self.send_cmd(13, bright)
        self.send_cmd(14, mode)
        self.send_cmd(15, interval)
        self.send_cmd(16, step)
        self.send_cmd(999, 2) 

    def apply_keys_realtime(self, prefix, is_double):
        mode_val = getattr(self, f"{prefix}_mode").get()
        combo_vals = getattr(self, f"{prefix}_combos")
        text_val = getattr(self, f"{prefix}_text").get()

        if is_double:
            self.send_cmd(4, 2)
            mode_idx, combo_start, text_start = 53, 54, 59
        else:
            mode_idx, combo_start, text_start = 17, 18, 23

        self.send_cmd(mode_idx, mode_val)
        for i in range(5):
            self.send_cmd(combo_start + i, KEY_MAP.get(combo_vals[i].get(), 0))
            
        text_str = text_val.ljust(30, '\0')
        for i in range(30):
            self.send_cmd(text_start + i, ord(text_str[i]) if text_str[i] != '\0' else 0)

    def save_to_eeprom(self):
        if not self.serial_port or not self.serial_port.is_open:
            return messagebox.showwarning("警告", "设备未连接！")
        self.send_cmd(999, 0)
        messagebox.showinfo("成功", "所有配置已成功保存至键盘")

if __name__ == "__main__":
    root = tk.Tk()
    try:
        from ctypes import windll
        windll.shcore.SetProcessDpiAwareness(1)
    except: pass
    app = CaptainKey(root)
    root.mainloop()