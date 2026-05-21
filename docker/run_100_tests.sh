#!/bin/sh

set -eu

cd /work/udpCode_spring26_v3

make

rm -rf test_runs
mkdir -p test_runs/input
mkdir -p test_runs/output
mkdir -p test_runs/log

pass_count=0
fail_count=0

run_one_test()
{
	test_num=$1
	port=$2
	window_size=$3
	buffer_size=$4
	error_rate=$5
	file_size=$6

	in_file="test_runs/input/input_${test_num}.bin"
	out_file="test_runs/output/output_${test_num}.bin"
	server_log="test_runs/log/server_${test_num}.log"
	rcopy_log="test_runs/log/rcopy_${test_num}.log"

	rm -f "$in_file" "$out_file" "$server_log" "$rcopy_log"

	if [ "$file_size" -eq 0 ]; then
		: > "$in_file"
	else
		dd if=/dev/urandom of="$in_file" bs=1 count="$file_size" status=none
	fi

	./server "$error_rate" "$port" > "$server_log" 2>&1 &
	server_pid=$!

	sleep 1

	if timeout 30s ./rcopy "$in_file" "$out_file" "$window_size" \
			"$buffer_size" "$error_rate" localhost "$port" \
			> "$rcopy_log" 2>&1; then
		rcopy_status=0
	else
		rcopy_status=$?
	fi

	kill "$server_pid" 2>/dev/null || true
	wait "$server_pid" 2>/dev/null || true

	if [ "$rcopy_status" -eq 0 ] && [ -f "$out_file" ] &&
			cmp -s "$in_file" "$out_file"; then
		echo "PASS test=$test_num window=$window_size buffer=$buffer_size error=$error_rate size=$file_size"
		pass_count=$((pass_count + 1))
	else
		echo "FAIL test=$test_num window=$window_size buffer=$buffer_size error=$error_rate size=$file_size"
		fail_count=$((fail_count + 1))
	fi
}

test_num=1

while [ "$test_num" -le 100 ]; do
	port=$((41000 + test_num))
	window_size=$((1 + (test_num % 10)))

	case $((test_num % 6)) in
		0) buffer_size=1 ;;
		1) buffer_size=64 ;;
		2) buffer_size=128 ;;
		3) buffer_size=512 ;;
		4) buffer_size=1000 ;;
		*) buffer_size=1400 ;;
	esac

	case $((test_num % 10)) in
		0) file_size=0 ;;
		1) file_size=1 ;;
		2) file_size=63 ;;
		3) file_size=1400 ;;
		4) file_size=1401 ;;
		5) file_size=4096 ;;
		6) file_size=8192 ;;
		7) file_size=16384 ;;
		8) file_size=32768 ;;
		*) file_size=65536 ;;
	esac

	if [ "$test_num" -le 80 ]; then
		error_rate=0
	else
		error_rate=0.01
	fi

	run_one_test "$test_num" "$port" "$window_size" \
		"$buffer_size" "$error_rate" "$file_size"

	test_num=$((test_num + 1))
done

echo "TOTAL pass=$pass_count fail=$fail_count"

if [ "$fail_count" -ne 0 ]; then
	exit 1
fi
