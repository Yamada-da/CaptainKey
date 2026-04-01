# CaptainKey
### 一键拯救绝望机长 有效防止坠机<br />
#### 基于OneKeyBoard_V2.1制作的修改版固件及本地上位机程序<br />
感谢原作者Anbin的优秀开源项目<br />
详见：https://oshwhub.com/anbin/fish-custom-single-key-keypad-ch552<br />
<br />
#### 项目说明：<br />
PCB通用于原开源项目<br />
实现了本地版上位机控制程序<br />
固件内增加了多媒体按键的支持<br />
重新绘制了带tpu缓冲夹心的3D外壳（后续将开源至Makerworld）<br />
<br />
#### 默认按键配置为：<br />
单击：静音 + 显示桌面<br />
双击：锁屏<br />
可使用上位机程序进行更改<br />
可支持配置灯光模式、文本输入<br />
<br />
#### 一些补充说明：<br />
##### 顺序组合及倒序组合：<br />
两种组合源于原作者在固件中留下的两种模式，在固件源码中，这两种模式在按下时都是顺序按下。<br />
区别在于释放时：倒序组合（模式2）会从最后一个按键开始往前逐个释放（例如按下 Ctrl -> Shift -> A，释放时是 释放A -> 释放Shift -> 释放Ctrl），一般使用上没有区别，主要目的是某些情况下防止出现快捷键粘滞。<br />
##### 固件版本区别：<br />
CaptianKey_vX.hex : 此版本为计数版，固件将每120秒向EEPROM写入目前按键总次数<br />
CaptainKey_vX_WithoutClickCount.hex ： 如果你在意EEPROM寿命，请刷入此版本，按键计数将只保存在运存中，断电即重置，且不写入EEPROM<br />
##### 刷机说明：<br />
全新未刷入固件的芯片，插入后即进入刷机模式。如已刷入固件请短接PCB正面（机械轴下方）的两个金属触点的同时，插入USB刷机。<br />
<br />
两种刷机方法：<br />
1. 使用WCHISPStudio<br />
   a. 点击目标程序文件1，选定xxx.hex固件<br />
   b. 勾选启用代码和数据保护模式、使能RST Pin作为手工复位输入引脚、下载完成后运行目标程序、清空DataFlash、清空CodeFlash、串口免按键下载功能<br />
   c. 选择下载配置脚 P3.6<br />
   d. 点击下载<br />
2. 使用源码通过Arduino IDE编译刷入<br />
   a. 点击文件>首选项 在其他开发板管理器地址中填入https://raw.githubusercontent.com/DeqingSun/ch55xduino/ch55xduino/package_ch55xduino_mcs51_index.json<br />
   b. 在工具>开发板>开发板管理器 中搜索CH55x并安装CH55xDuino<br />
   c. 在工具>开发板>CH55xDuino 中找到并选择CH552<br />
   d. 点击工具>开发板>USB Settings>USER CODE w/ 148B USB ram<br />
   e. 现在你可以编辑源文件（如果需要）后点击左上角 "→" 箭头上传即可<br />

#### 上位机截图：<br />
![image](https://github.com/Yamada-da/CaptainKey/blob/main/Screenshot/CaptainKey.png)
