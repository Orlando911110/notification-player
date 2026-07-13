package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"notification-player/internal/config"
	"notification-player/internal/player"
	"notification-player/internal/server"
)

var (
	configPath = flag.String("config", "/etc/notification-player/config.yaml", "配置文件路径")
	version    = flag.Bool("version", false, "显示版本信息")
)

// 构建时注入的变量
var (
	Version   = "dev"
	BuildTime = "unknown"
	GitCommit = "unknown"
)

func main() {
	flag.Parse()

	if *version {
		fmt.Printf("Notification Player\n")
		fmt.Printf("Version: %s\n", Version)
		fmt.Printf("Build Time: %s\n", BuildTime)
		fmt.Printf("Git Commit: %s\n", GitCommit)
		os.Exit(0)
	}

	// 加载配置
	cfg, err := config.Load(*configPath)
	if err != nil {
		log.Printf("警告: 加载配置文件失败: %v, 使用默认配置", err)
		cfg = config.DefaultConfig()
	}

	// 创建音频播放器
	audioPlayer, err := player.NewAudioPlayer(cfg)
	if err != nil {
		log.Fatalf("创建音频播放器失败: %v", err)
	}
	defer audioPlayer.Stop()

	// 创建HTTP服务器
	srv := server.NewServer(cfg, audioPlayer)

	// 启动HTTP服务
	addr := fmt.Sprintf("%s:%d", cfg.Server.Host, cfg.Server.Port)
	httpServer := &http.Server{
		Addr:         addr,
		Handler:      srv,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 10 * time.Second,
	}

	// 优雅关闭
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		log.Printf("========================================")
		log.Printf("Notification Player v%s", Version)
		log.Printf("监听地址: http://%s", addr)
		log.Printf("API端点: http://%s/api/notify", addr)
		log.Printf("状态端点: http://%s/api/status", addr)
		log.Printf("========================================")

		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("启动服务失败: %v", err)
		}
	}()

	<-sigChan
	log.Println("收到退出信号，正在关闭服务...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := httpServer.Shutdown(ctx); err != nil {
		log.Fatalf("关闭服务失败: %v", err)
	}

	log.Println("服务已关闭")
}