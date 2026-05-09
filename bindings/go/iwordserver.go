package iword

// Client connects to a running iwordserver over Unix socket or TCP and
// exposes the same API surface as the CGO functions in this package.
//
// All methods are safe for concurrent use — the server serializes iword
// calls internally, so no additional locking is required on the client side.
//
// Typical usage:
//
//	c, err := NewClient("unix", "/tmp/iword.sock")
//	if err != nil { ... }
//	defer c.Close()
//
//	key, err := c.Seek("free")
//	matches, err := c.Map("get free prize now", ModeHTML|ModeForbid)

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"sync"
)

// Client is a persistent connection to iwordserver.
type Client struct {
	conn   net.Conn
	enc    *json.Encoder
	dec    *json.Decoder
	mu     sync.Mutex
}

// NewClient dials an iwordserver.
// network is "unix" or "tcp"; address is the socket path or host:port.
func NewClient(network, address string) (*Client, error) {
	conn, err := net.Dial(network, address)
	if err != nil {
		return nil, fmt.Errorf("iwordserver: dial %s %s: %w", network, address, err)
	}
	return &Client{
		conn: conn,
		enc:  json.NewEncoder(conn),
		dec:  json.NewDecoder(bufio.NewReader(conn)),
	}, nil
}

// Close closes the connection to iwordserver.
func (c *Client) Close() error {
	return c.conn.Close()
}

// Ping verifies the connection is alive.
func (c *Client) Ping() error {
	var resp struct {
		Pong bool `json:"pong"`
	}
	if err := c.call(map[string]any{"op": "ping"}, &resp); err != nil {
		return err
	}
	if !resp.Pong {
		return fmt.Errorf("iwordserver: unexpected ping response")
	}
	return nil
}

// Seek looks up a word in the dictionary.
// Returns the category key (0–14), or -1 if not found.
func (c *Client) Seek(word string) (int, error) {
	var resp struct {
		Found bool `json:"found"`
		Key   *int `json:"key"`
		Error string `json:"error"`
	}
	if err := c.call(map[string]any{"op": "seek", "word": word}, &resp); err != nil {
		return -1, err
	}
	if resp.Error != "" {
		return -1, fmt.Errorf("iwordserver: seek: %s", resp.Error)
	}
	if !resp.Found || resp.Key == nil {
		return -1, nil
	}
	return *resp.Key, nil
}

// Map extracts all matching words from text.
// mode is a combination of ModeHTML, ModeForbid, ModeEnglish.
func (c *Client) Map(text string, mode int) ([]Match, error) {
	var resp struct {
		Matches []struct {
			Pos int `json:"pos"`
			Len int `json:"len"`
			Key int `json:"key"`
		} `json:"matches"`
		Mask  int    `json:"mask"`
		Error string `json:"error"`
	}
	if err := c.call(map[string]any{"op": "map", "text": text, "mode": mode}, &resp); err != nil {
		return nil, err
	}
	if resp.Error != "" {
		return nil, fmt.Errorf("iwordserver: map: %s", resp.Error)
	}
	if len(resp.Matches) == 0 {
		return nil, nil
	}
	matches := make([]Match, len(resp.Matches))
	for i, m := range resp.Matches {
		matches[i] = Match{Position: m.Pos, Length: m.Len, Key: m.Key}
	}
	return matches, nil
}

// Mask returns a bitmask of category keys present in the loaded dictionary.
func (c *Client) Mask() (int, error) {
	var resp struct {
		Mask  int    `json:"mask"`
		Error string `json:"error"`
	}
	if err := c.call(map[string]any{"op": "mask"}, &resp); err != nil {
		return 0, err
	}
	if resp.Error != "" {
		return 0, fmt.Errorf("iwordserver: mask: %s", resp.Error)
	}
	return resp.Mask, nil
}

// FilterText replaces all matched words in text with '*' characters.
func (c *Client) FilterText(text string, mode int) (string, error) {
	matches, err := c.Map(text, mode)
	if err != nil {
		return "", err
	}
	if len(matches) == 0 {
		return text, nil
	}
	buf := []byte(text)
	for _, m := range matches {
		for i := 0; i < m.Length; i++ {
			buf[m.Position+i] = '*'
		}
	}
	return string(buf), nil
}

// call sends one request and decodes the response. Serialized by mu.
func (c *Client) call(req map[string]any, resp any) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	if err := c.enc.Encode(req); err != nil {
		return fmt.Errorf("iwordserver: send: %w", err)
	}
	if err := c.dec.Decode(resp); err != nil {
		return fmt.Errorf("iwordserver: recv: %w", err)
	}
	return nil
}
