package server

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"notification-player/internal/config"
	"notification-player/internal/player"
)

type Server struct {
	config *config.Config
	player *player.AudioPlayer
}

func NewServer(cfg *config.Config, p *player.AudioPlayer) *Server {
	s := &Server{
		config: cfg,
		player: p,
	}

	mux := http.NewServeMux()
	mux.HandleFunc("/api/notify", s.handleNotify)
	mux.HandleFunc("/api/status", s.handleStatus)
	mux.HandleFunc("/health", s.handleHealth)
	mux.HandleFunc("/", s.handleRoot)

	return s
}

func (s *Server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	// 认证检查
	if s.config.Security.Token != "" {
		token := r.Header.Get("Authorization")
		if token != s.config.Security.Token {
			http.Error(w, "未授权: Token无效", http.StatusUnauthorized)
			return
		}
	}

	http.DefaultServeMux.ServeHTTP(w, r)
}

func (s *Server) handleNotify(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "方法不允许", http.StatusMethodNotAllowed)
		return
	}

	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "读取请求失败", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	var req struct {
		Message  string            `json:"message"`
		Sound    string            `json:"sound"`
		Priority string            `json:"priority"`
		Metadata map[string]string `json:"metadata"`
	}

	if err := json.Unmarshal(body, &req); err != nil {
		http.Error(w, fmt.Sprintf("解析请求失败: %v", err), http.StatusBadRequest)
		return
	}

	if req.Message == "" {
		http.Error(w, "消息不能为空", http.StatusBadRequest)
		return
	}

	notification := &player.NotificationRequest{
		Message:  req.Message,
		Sound:    req.Sound,
		Priority: req.Priority,
		Metadata: req.Metadata,
	}

	go s.player.Play(notification)

	resp := map[string]interface{}{
		"success": true,
		"message": "通知已加入播放队列",
		"id":      fmt.Sprintf("notify-%d", time.Now().UnixNano()),
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusAccepted)
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, "方法不允许", http.StatusMethodNotAllowed)
		return
	}

	status := s.player.Status()
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(status)
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(http.StatusOK)
	w.Write([]byte("OK"))
}

func (s *Server) handleRoot(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/" {
		http.NotFound(w, r)
		return
	}

	w.Header().Set("Content-Type", "text/html")
	fmt.Fprint(w, `
<!DOCTYPE html>
<html>
<head>
    <title>Notification Player</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
        h1 { color: #333; }
        .endpoint { background: #f5f5f5; padding: 15px; margin: 10px 0; border-radius: 5px; }
        code { background: #e8e8e8; padding: 2px 6px; border-radius: 3px; }
        pre { background: #f5f5f5; padding: 15px; border-radius: 5px; overflow-x: auto; }
    </style>
</head>
<body>
    <h1>🔊 Notification Player</h1>
    <p>HTTP API 远程音频通知播放服务</p>
    
    <div class="endpoint">
        <h3>📡 API 端点</h3>
        <ul>
            <li><strong>POST</strong> <code>/api/notify</code> - 发送通知</li>
            <li><strong>GET</strong> <code>/api/status</code> - 查询状态</li>
            <li><strong>GET</strong> <code>/health</code> - 健康检查</li>
        </ul>
    </div>

    <div class="endpoint">
        <h3>📝 使用示例</h3>
        <pre>
curl -X POST http://localhost:8080/api/notify \
  -H "Content-Type: application/json" \
  -d '{
    "message": "新订单通知",
    "sound": "order.wav"
  }'
        </pre>
    </div>
</body>
</html>
`)
}