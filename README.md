# PlayerClient - 消息通知播放客户端

支持 HTTP API 远程触发音频播放的桌面通知客户端，专为 MIPS64 架构设计。

## 功能特点

- 🎵 **HTTP API 控制**：通过 RESTful API 远程触发音频播放
- 🔊 **多格式支持**：支持 MP3 和 WAV 音频格式
- 📢 **桌面通知**：集成系统桌面通知
- 🖥️ **系统托盘**：提供系统托盘图标快速访问
- 🚀 **开机自启**：通过 systemd 服务实现开机自动启动
- 🎚️ **音量控制**：独立的音量控制功能
- 🌐 **CORS 支持**：支持跨域请求

## API 接口

### 播放音频
```bash
POST /api/play
Content-Type: application/json

{
    "file": "notification.wav",
    "volume": 80
}