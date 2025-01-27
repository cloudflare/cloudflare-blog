#!/bin/bash

for i in {1..10}
do
  curl localhost:8787 -d "$i"
  ((i=i+1))
  sleep 1
done