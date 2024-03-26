#!/bin/sh

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
SET input_format_import_nested_json = 1;
' | clickhouse-client

echo -n '
SET output_format_json_array_of_rows = 1;
' | clickhouse-client

echo -n "SET date_time_input_format='best_effort';" | clickhouse-client

echo -n '
CREATE TABLE r0.alerts
(
    `date` Date DEFAULT toDate(now()),
    `datetime` DateTime DEFAULT now(),
    `timestamp` DateTime64(3) DEFAULT now() CODEC(Delta(4), ZSTD(1)),
    `startsAt` DateTime64(3),
    `endsAt` DateTime64(3),
    `updatedAt` DateTime64(3),
    `status.inhibitedBy` Array(String),
    `status.silencedBy` String,
    `status.state` LowCardinality(String),
    `annotations.summary` String,
    `annotations.dashboard` String,
    `annotations.link` String,
    `fingerprint` String,
    `receivers` Array(String),
    `labelsmap` Map(String, String),
    `labels.alertname` String,
    `labels.component` String,
    `labels.service` String,
    `labels.instance` String,
    `labels.job` String,
    `labels.metal` String,
    `labels.notify` String,
    `labels.priority` String,
    `labels.prometheus` String,
    `labels.region` String,
    `labels.severity` String
)
ENGINE = MergeTree
PARTITION BY toStartOfHour(datetime)
ORDER BY labels.alertname
SETTINGS index_granularity = 8192;' | clickhouse-client

echo -n '
CREATE TABLE r0.silences
(
    `date` Date DEFAULT toDate(now()),
    `datetime` DateTime DEFAULT now(),
    `id` String,
    `status.state` LowCardinality(String),
    `updatedAt` DateTime64(3),
    `startsAt` DateTime64(3),
    `createdBy` LowCardinality(String),
    `endsAt` DateTime64(3),
    `matchers` Map(String, String),
    `comment` String
)
ENGINE = ReplacingMergeTree
PARTITION BY toStartOfHour(datetime)
ORDER BY (id, startsAt, endsAt)
SETTINGS index_granularity = 8192;
' | clickhouse-client
