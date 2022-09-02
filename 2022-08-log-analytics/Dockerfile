FROM grafana/grafana:9.1.0-ubuntu

WORKDIR /tmp
USER root

RUN curl -vkL https://github.com/grafana/clickhouse-datasource/releases/download/v1.1.2/grafana-clickhouse-datasource-1.1.2.linux_amd64.zip -o grafana-clickhouse-datasource-1.1.2.linux_amd64.zip

RUN apt-get update && apt-get install -y unzip && \
    unzip grafana-clickhouse-datasource-1.1.2.linux_amd64.zip -d /var/lib/grafana/plugins/

COPY dashboards/demo-logs.json  /var/lib/grafana/dashboards/
