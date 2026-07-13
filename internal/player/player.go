package player

import (
	"context"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"sync"

	"notification-player/internal/config"
)

type NotificationRequest struct {
	Message  string
	Sound    string
	Priority string
	Metadata map[string]string
}

type AudioPlayer struct {
	config    *config.Config
	mu        sync.Mutex
	queue     chan *NotificationRequest
	ctx       context.Context
	cancel    context.CancelFunc
	wg        sync.WaitGroup
	isPlaying bool
}

func NewAudioPlayer(cfg *config.Config) (*AudioPlayer, error) {
	// 检查音频设备
	if err := checkAudioDevice(); err != nil {
		log.Printf("警告: 音频设备检查失败: %v", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	player := &AudioPlayer{
		config: cfg,
		queue:  make(chan *NotificationRequest, 100),
		ctx:    ctx,
		cancel: cancel,
	}

	player.wg.Add(1)
	go player.processQueue()

	return player, nil
}

func (p *AudioPlayer) processQueue() {
	defer p.wg.Done()

	for {
		select {
		case <-p.ctx.Done():
			return
		case req := <-p.queue:
			p.isPlaying = true
			log.Printf("[通知] 消息: %s, 音频: %s", req.Message, req.Sound)

			if err := p.playSound(req.Sound); err != nil {
				log.Printf("[错误] 播放失败: %v", err)
			}

			p.isPlaying = false
		}
	}
}

func (p *AudioPlayer) playSound(soundFile string) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	// 确定音频文件路径
	soundPath := p.config.GetSoundPath(soundFile)
	if _, err := os.Stat(soundPath); os.IsNotExist(err) {
		// 尝试使用默认音频
		soundPath = p.config.GetSoundPath(p.config.Audio.Default)
		if _, err := os.Stat(soundPath); os.IsNotExist(err) {
			return fmt.Errorf("音频文件不存在: %s", soundPath)
		}
	}

	// 选择播放器
	player := p.config.Audio.Player
	var cmd *exec.Cmd

	switch player {
	case "aplay":
		cmd = exec.Command("aplay", "-q", soundPath)
	case "paplay":
		cmd = exec.Command("paplay", soundPath)
	case "mpv":
		cmd = exec.Command("mpv", "--no-video", "--really-quiet", soundPath)
	case "ffplay":
		cmd = exec.Command("ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", soundPath)
	default:
		cmd = autoDetectPlayer(soundPath)
	}

	if cmd == nil {
		return fmt.Errorf("未找到可用的音频播放器")
	}

	log.Printf("[播放] 执行: %s", cmd.String())

	// 设置音量
	if p.config.Audio.Volume > 0 {
		setVolume(p.config.Audio.Volume)
	}

	return cmd.Run()
}

func (p *AudioPlayer) Play(req *NotificationRequest) {
	select {
	case p.queue <- req:
		log.Printf("[队列] 已加入: %s (队列长度: %d)", req.Message, len(p.queue))
	default:
		log.Printf("[警告] 队列已满，丢弃通知: %s", req.Message)
	}
}

func (p *AudioPlayer) Stop() {
	p.cancel()
	p.wg.Wait()
}

func (p *AudioPlayer) Status() map[string]interface{} {
	return map[string]interface{}{
		"is_playing": p.isPlaying,
		"queue_size": len(p.queue),
		"queue_cap":  cap(p.queue),
	}
}

func checkAudioDevice() error {
	if _, err := os.Stat("/dev/snd"); os.IsNotExist(err) {
		return fmt.Errorf("音频设备不存在")
	}
	return nil
}

func autoDetectPlayer(soundPath string) *exec.Cmd {
	players := []struct {
		name string
		cmd  func(string) *exec.Cmd
	}{
		{"aplay", func(path string) *exec.Cmd {
			return exec.Command("aplay", "-q", path)
		}},
		{"paplay", func(path string) *exec.Cmd {
			return exec.Command("paplay", path)
		}},
		{"mpv", func(path string) *exec.Cmd {
			return exec.Command("mpv", "--no-video", "--really-quiet", path)
		}},
		{"ffplay", func(path string) *exec.Cmd {
			return exec.Command("ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", path)
		}},
	}

	for _, p := range players {
		if _, err := exec.LookPath(p.name); err == nil {
			return p.cmd(soundPath)
		}
	}

	return nil
}

func setVolume(volume int) {
	if volume < 0 || volume > 100 {
		return
	}

	if _, err := exec.LookPath("amixer"); err == nil {
		cmd := exec.Command("amixer", "sset", "Master", fmt.Sprintf("%d%%", volume))
		cmd.Run()
	}

	if _, err := exec.LookPath("pactl"); err == nil {
		cmd := exec.Command("pactl", "set-sink-volume", "@DEFAULT_SINK@", fmt.Sprintf("%d%%", volume))
		cmd.Run()
	}
}