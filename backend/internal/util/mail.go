package util

import (
	"encoding/base64"
	"fmt"
	"net/smtp"
)

type SMTPClient struct {
	config *SMTPConfig
}

type SMTPConfig struct {
	Host     string `json:"host" env:"SMTP_HOST,required"`
	Port     string `json:"port" env:"SMTP_PORT,required"`
	Username string `json:"username" env:"SMTP_USERNAME,required"`
	Password string `json:"password" env:"SMTP_PASSWORD,required"`
	From     string `json:"from" env:"SMTP_FROM,required"`
}

func NewSMTPClient(cfg *SMTPConfig) *SMTPClient {
	return &SMTPClient{
		config: cfg,
	}
}

func (e *SMTPClient) SendEmail(toEmail string, subjectRaw string, body string) error {
	subject := "=?UTF-8?B?" + base64.StdEncoding.EncodeToString([]byte(subjectRaw)) + "?="
	message := "From: Test <" + e.config.Username + ">\r\n" +
		"To: " + toEmail + "\r\n" +
		"Subject: " + subject + "\r\n" +
		"MIME-Version: 1.0\r\n" +
		"Content-Type: text/plain; charset=UTF-8\r\n" +
		"\r\n" +
		body

	// Аутентификация и отправка
	auth := smtp.PlainAuth("", e.config.Username, e.config.Password, e.config.Host)
	err := smtp.SendMail(
		e.config.Host+":"+e.config.Port,
		auth,
		e.config.From,
		[]string{toEmail},
		[]byte(message),
	)

	if err != nil {
		fmt.Println("Ошибка отправки:", err, message)
		return err
	} else {
		fmt.Println("Письмо успешно отправлено! ")
		return nil
	}
}
