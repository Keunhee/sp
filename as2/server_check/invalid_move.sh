#!/bin/bash
while IFS= read -r line; do
  echo "$line"
  sleep 4
done < invalid_move.jsonl | nc 127.0.0.1 8888