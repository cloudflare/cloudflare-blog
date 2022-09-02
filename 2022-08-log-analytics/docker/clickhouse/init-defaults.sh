#!/bin/bash

CLICKHOUSE_DB="${CLICKHOUSE_DB:-database}";
CLICKHOUSE_USER="${CLICKHOUSE_USER:-user}";
CLICKHOUSE_PASSWORD="${CLICKHOUSE_PASSWORD:-password}";

cat <<EOT >> /etc/clickhouse-server/users.d/user.xml
<yandex>
  <!-- Docs: <https://clickhouse.tech/docs/en/operations/settings/settings_users/> -->
  <users>
    <${CLICKHOUSE_USER}>
      <profile>default</profile>
      <networks>
        <ip>::/0</ip>
      </networks>
      <password>${CLICKHOUSE_PASSWORD}</password>
      <quota>default</quota>
    </${CLICKHOUSE_USER}>
  </users>
</yandex>
EOT
#cat /etc/clickhouse-server/users.d/user.xml;

clickhouse-client --query "CREATE DATABASE IF NOT EXISTS ${CLICKHOUSE_DB}";

echo -n '
CREATE TABLE r0.logs
(
    `bytes` UInt8,
    `event_time`  DateTime,
    `host` String,
    `method` String,
    `protocol` String,
    `referer` String,
    `request` String,
    `status` String,
    `user-identifier` String,
)
ENGINE = MergeTree
PARTITION BY toStartOfHour(event_time)
ORDER BY (event_time)
SETTINGS index_granularity = 8192
;' | clickhouse-client
