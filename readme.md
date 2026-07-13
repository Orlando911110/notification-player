# 🔊 Notification Player

专为 **Loongson 3A4000 MIPS64el** 架构优化的 HTTP API 远程音频通知播放服务。

[![Go Version](https://img.shields.io/badge/Go-1.16+-00ADD8?style=flat&logo=go)](https://golang.org)
[![Arch](https://img.shields.io/badge/Arch-MIPS64el-blue)](https://www.loongson.cn)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

## ✨ 特性

- 🚀 **轻量级** - 无外部依赖，纯 Go 实现
- 🎯 **专为龙芯优化** - 在 Loongson 3A4000 MIPS64el 上测试通过
- 🔊 **多播放器支持** - aplay, paplay, mpv, ffplay, gst-launch
- 📡 **HTTP API** - 支持远程触发音频播放
- 🔐 **认证支持** - 可选的 Token 认证
- 📊 **队列管理** - 防止音频冲突，支持优先级
- 🎚️ **音量控制** - 软件音量调节
- 🐧 **系统服务** - 支持 Systemd 管理

## 📋 系统要求

- **架构**: MIPS64el (龙芯3A4000) / x86_64 / ARM64
- **系统**: Linux (Debian/Ubuntu/CentOS)
- **Go**: 1.16 或更高版本
- **音频**: ALSA / PulseAudio

## 🚀 快速开始

### 1. 克隆仓库

```bash
git clone https://github.com/yourusername/notification-player.git
cd notification-player