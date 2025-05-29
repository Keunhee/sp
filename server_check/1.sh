#!/bin/bash
while IFS= read -r line; do
  echo "$line"
  sleep 2
done < alice.jsonl | nc 0.0.0.0 8888