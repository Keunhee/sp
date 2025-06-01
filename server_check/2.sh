#!/bin/bash
while IFS= read -r line; do
  echo "$line"
  sleep 4
done < bob.jsonl | nc 127.0.0.1 8888