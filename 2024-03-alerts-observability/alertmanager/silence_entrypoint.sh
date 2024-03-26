#!/bin/sh

echo "{
      \"matchers\": [
        {
          \"name\": \"alertname\",
          \"value\": \"service_down\",
          \"isRegex\": false
        }
      ],
      \"startsAt\": \"$(date -Ins | sed s/+00:00/Z/ | sed s/,/./)\",
      \"endsAt\": \"$(TZ='UTC-1:00' date -Ins | sed s/+01:00/Z/ | sed s/,/./)\",
      \"createdBy\": \"demouser\",
      \"comment\": \"Silence\"
}" > post-data
sleep 90

wget 'http://alertmanager:9093/api/v1/silences' --header='Content-Type: application/json' --post-file=post-data
