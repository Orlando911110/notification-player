package config

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

type Config struct {
	Server   ServerConfig   `yaml:"server"`
	Audio    AudioConfig    `yaml:"audio"`
	Security SecurityConfig `yaml:"security"`
}

type ServerConfig struct {
	Host string `yaml:"host"`
	Port int    `yaml:"port"`
}

type AudioConfig struct {
	Player   string `yaml:"player"`
	SoundDir string `yaml:"sound_dir"`
	Default  string `yaml:"default"`
	Volume   int    `yaml:"volume"`
}

type SecurityConfig struct {
	Token string `yaml:"token"`
}

func DefaultConfig() *Config {
	return &Config{
		Server: ServerConfig{
			Host: "0.0.0.0",
			Port: 8080,
		},
		Audio: AudioConfig{
			Player:   "aplay",
			SoundDir: "/usr/share/notification-player/sounds",
			Default:  "default.wav",
			Volume:   80,
		},
		Security: SecurityConfig{
			Token: "",
		},
	}
}

func Load(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	cfg := DefaultConfig()
	
	// 简化的YAML解析（无外部依赖）
	lines := strings.Split(string(data), "\n")
	currentSection := ""

	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}

		if strings.HasSuffix(line, ":") && !strings.Contains(line, " ") {
			currentSection = strings.TrimSuffix(line, ":")
			continue
		}

		if strings.Contains(line, ":") {
			parts := strings.SplitN(line, ":", 2)
			key := strings.TrimSpace(parts[0])
			value := strings.TrimSpace(parts[1])
			value = strings.Trim(value, "\"'")

			switch currentSection {
			case "server":
				switch key {
				case "host":
					cfg.Server.Host = value
				case "port":
					fmt.Sscanf(value, "%d", &cfg.Server.Port)
				}
			case "audio":
				switch key {
				case "player":
					cfg.Audio.Player = value
				case "sound_dir":
					cfg.Audio.SoundDir = value
				case "default":
					cfg.Audio.Default = value
				case "volume":
					fmt.Sscanf(value, "%d", &cfg.Audio.Volume)
				}
			case "security":
				switch key {
				case "token":
					cfg.Security.Token = value
				}
			}
		}
	}

	// 确保目录存在
	if err := os.MkdirAll(cfg.Audio.SoundDir, 0755); err != nil {
		return nil, err
	}

	return cfg, nil
}

func (c *Config) GetSoundPath(sound string) string {
	if sound == "" {
		sound = c.Audio.Default
	}
	return filepath.Join(c.Audio.SoundDir, sound)
}