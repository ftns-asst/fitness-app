# Data

Инфраструктурный слой: HTTP, DTO-маппинг, SQLite, token store и реализации
репозиториев.

`Network/` содержит API-ядро issue 5:

- `AsHttp_Client` — единый `QNetworkAccessManager`, request/response DTO,
  transfer timeout и настраиваемый exponential retry для network/5xx;
- `AApi_Error` — backend `{ key, message }` с fallback-маппингом transport/HTTP
  ошибок.

API-адаптеры решают, можно ли повторять конкретный запрос: generic GET включает
retry, `Post_JSON` по умолчанию его запрещает. Auto-refresh и token store не
входят в HTTP core и добавляются issue 7.

Зависит от Qt и внешних систем; не содержит визуального QML.