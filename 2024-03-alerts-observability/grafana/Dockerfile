FROM grafana/grafana:10.2.3-ubuntu

WORKDIR /tmp
USER root

RUN grafana cli plugins install grafana-clickhouse-datasource

RUN mkdir -p /var/lib/grafana/dashboards

COPY dashboards/*.json  /var/lib/grafana/dashboards/
