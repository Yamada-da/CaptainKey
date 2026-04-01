# CaptainKey
### 拯救每个绝望机长<br />
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
#### 关于顺序组合及倒序组合说明：<br />
两种组合源于原作者在固件中留下的两种模式，在固件源码中，这两种模式在按下时都是顺序按下。<br />
区别在于释放时：倒序组合（模式2）会从最后一个按键开始往前逐个释放（例如按下 Ctrl -> Shift -> A，释放时是 释放A -> 释放Shift -> 释放Ctrl），一般使用上没有区别，主要目的是某些情况下防止出现快捷键粘滞。

#### 上位机截图：
![image](https://github.com/Yamada-da/CaptainKey/blob/main/Screenshot/CaptainKey.png)
