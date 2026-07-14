# PlayerClient - MIPS64 音频播放客户端

轻量级 HTTP API 音频播放服务器，专为 MIPS64 架构优化。

## 特点

- ✅ 零外部依赖，纯 C 语言实现
- ✅ 静态编译，单一可执行文件
- ✅ RESTful HTTP API
- ✅ 支持 ALSA/PulseAudio/SoX
- ✅ 音量控制和配置文件
- ✅ systemd 服务支持

## API 端点

### 获取状态
```bash
curl http://localhost:8080/api/status
