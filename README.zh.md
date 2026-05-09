# voxstick

> 一根插 Mac 就能用的 **中英语音输入棒**，专门跟**微信输入法**搭配丝滑。
> [English README](README.md)

把麦克风和按键塞进 USB 棒里。**按一下说话，再按一下停，长按一下发送**。
所有事都靠系统自带的输入法（推荐微信输入法）和 USB 标准协议完成
——**不装任何驱动，不装任何 menu bar app**。

```
[USB-C] → 棒 → Mac
         │
         ├ 16 kHz 麦克风 (USB Audio Class)
         └ 蓝牙级别的真键盘 (USB HID)：⌘+F12 / Enter
```

## 这跟微信输入法有什么关系

微信输入法 Mac 版有两个**杀手级特性**用作"无线智能听写器"再合适不过：

1. **中英自由混说** —— 程序员说"把这个 PR merge 一下" / "用 React useEffect 做个 hook"，
   它能正确识别中文 + 保留英文术语原拼写，比 macOS 自带 Dictation 强一截。
2. **LLM 驱动的实时纠错** —— 你说话边说边断句、加标点、改错别字，最后把
   整段已经"洗干净"的文本写进光标位置。这个能力在通用 ASR 工具里少见。

但用微信输入法的语音听写有个不爽的地方：**它默认绑 fn 键**，外接键盘
（例如 USB 键盘 / 蓝牙键盘）按 fn 不一定灵。voxstick 解决了这个：

- **轻按棒上的大按键** = 发 `⌘+F12` HID 报告 → 微信输入法的"免提模式"绑这个键 → 进语音状态
- 边说话边看微信输入法的浮窗实时上字 / 智能纠错
- **再轻按一下** = 第二次发 `⌘+F12` → 微信输入法停录、把最终文本插进输入框
- **长按棒上的大按键** ≥ 0.6 秒 = 发 `Enter` → 把消息发出去

整个流程**不用碰键盘**。

## 关键卖点

- **手感对**：物理大按键，不是 fn 也不是软件 hotkey。按一下就一下，不会跟系统快捷键打架
- **隐私可控**：内置 6 轴 IMU。把棒**平放桌面 → 麦克风物理静音**，立起来 → 解禁
- **板载麦克风**：ES8311 codec + 高灵敏 MEMS 麦克风，远离笔记本风扇噪音
- **状态屏**：240×135 LCD 实时显示麦克风能量动态圆圈，按键时变色：
  - 平放 = 红色（已静音）
  - 立起空闲 = 绿色
  - 轻按中 = 品红
  - 长按蓝色后松开 = 发送 Enter
- **跨平台**：UAC + HID 是标准协议，Mac / Windows / Linux 都即插即用。Windows 上甚至更顺
  （Mac 上 F19/F18 这些键被系统/输入法软件名单拦了，Windows 没这些限制）

## 微信输入法配置（关键三步）

1. **设置 → 语音输入 → 免提模式 → 设置快捷键** → 按一下棒上的大按键 → 输入框捕获 `⌘+F12`，保存
2. **系统设置 → 隐私与安全 → 辅助功能** → 打开"微信输入法"开关（首次需要授权）
3. **任意聊天/编辑器** → 把光标点进文本框 → 轻按棒说话 → 再轻按结束 → 长按发送

完事。

## 硬件

[M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) — 大约 ¥150 / $25，预装一颗 ESP32-S3 + ES8311 codec + 锂电 + LCD + IMU。voxstick 的固件直接刷上去，不用焊接。

## 编译 & 烧录

```sh
. $IDF_PATH/export.sh
idf.py build flash
```

ESP-IDF 5.5+。第一次会自动通过 IDF Component Manager 拉取
`espressif/usb_device_uac` / `esp_codec_dev` / `espressif2022/bmi270`。

如果烧坏了进不去 download mode（对 M5StickS3 来说这事儿可能发生），看
[SESSION-NOTES.md](SESSION-NOTES.md) 的 "M5PM1 BOOT-pin brick recovery"
章节。或者跑 `bash tools/trigger-download.sh` 让运行中的固件自己重启进
ROM 下载模式。

## 不只配微信输入法

棒本身只是 USB 标准设备，实际上配任何一个能识别系统麦克风 + 能绑键盘 hotkey
的工具都成：

| Mac 工具 | 触发 | 备注 |
|---|---|---|
| **微信输入法** ⭐ | ⌘+F12 | 中英混合 + LLM 纠错最强 |
| macOS 原生 Dictation | F19 (重新绑定) | 系统自带，不需安装；中文一般 |
| [VoiceInk](https://github.com/Beingpax/VoiceInk) | F19 | 本地 whisper.cpp，开源免费 |
| [MacWhisper Pro](https://goodsnooze.com/macwhisper) | 自定义 | whisper-large + GPT 纠错，$19 |

如果用 macOS Dictation 或 VoiceInk，可以改一行代码把 BtnA 发的键改成
F19（更"无冲突"），但之后跟微信输入法绑定就有冲突。详细见
[main/main.c](main/main.c#L1100) 顶部 `HID_KEY_F12 / HID_MOD_RIGHT_GUI`
那段。

## 路线图

完成的：
- ✅ 复合 USB 描述符（UAC 麦 + HID 键盘）
- ✅ 物理 PTT 按键 + 长按发送
- ✅ IMU 平放静音
- ✅ LCD 状态显示

可能加的（看反馈）：
- ⏳ 拔 USB 自动深度睡眠（电池续航 24h+）
- ⏳ BLE PTT（拔了 USB 还能键盘 PTT）
- ⏳ Windows 测试 + 更顺的 hotkey 配置（Windows 没 F-key 黑名单）

## License

[MIT](LICENSE)

完整的开发踩坑过程在 [SESSION-NOTES.md](SESSION-NOTES.md)（PMIC L3B LDO 通电、
ES8311 ADC reference 默认配置 bug、M5PM1 BOOT 引脚锁死复位等问题的解决记录）。
