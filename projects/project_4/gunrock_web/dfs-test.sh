#!/bin/bash

SERVER_URL="http://localhost:8080"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

function test_case() {
    echo -e "${YELLOW}============================================${NC}"
    echo -e "${YELLOW}== $1 ==${NC}"
    echo -e "${YELLOW}============================================${NC}"
}

function check_status() {
    local expected=$1
    local cmd=$2
    local description=$3

    echo -e "\n${YELLOW}TEST: $description${NC}"
    echo "Command: $cmd"

    # Execute command and save status
    http_code=$(curl -s -o /dev/null -w "%{http_code}" $cmd)

    if [ "$http_code" = "$expected" ]; then
        echo -e "${GREEN}✓ Success: Got expected status $expected${NC}"
    else
        echo -e "${RED}✗ Failure: Expected $expected but got $http_code${NC}"
    fi
}

function check_content() {
    local expected="$1"
    local cmd="$2"
    local description="$3"

    echo -e "\n${YELLOW}TEST: $description${NC}"
    echo "Command: $cmd"

    # Execute command and save content
    response=$(curl -s $cmd)

    if [[ "$response" == *"$expected"* ]]; then
        echo -e "${GREEN}✓ Success: Response contains expected content${NC}"
    else
        echo -e "${RED}✗ Failure: Response doesn't contain expected content${NC}"
        echo -e "Expected to contain: $expected"
        echo -e "Actual: $response"
    fi
}

function create_temp_file() {
    local content="$1"
    local temp_file=$(mktemp)
    echo -n "$content" > "$temp_file"
    echo "$temp_file"
}

# Basic Tests
test_case "BASIC PUT/GET/DELETE OPERATIONS"

# Create a temporary file with content
temp_file=$(create_temp_file "Hello, world!")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/test.txt" "Create test file"
rm "$temp_file"

check_content "Hello, world!" "$SERVER_URL/ds3/test.txt" "Read test file"
check_status 200 "-X DELETE $SERVER_URL/ds3/test.txt" "Delete test file"
check_status 404 "$SERVER_URL/ds3/test.txt" "Verify file deleted"

# Directory Structure Tests
test_case "DIRECTORY STRUCTURE TESTS"

# Create a temporary file with content
temp_file=$(create_temp_file "Nested file content")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/dir1/dir2/nested.txt" "Create nested file"
rm "$temp_file"

check_content "dir1/" "$SERVER_URL/ds3/" "List root directory"
check_content "dir2/" "$SERVER_URL/ds3/dir1/" "List dir1 directory"
check_content "nested.txt" "$SERVER_URL/ds3/dir1/dir2/" "List dir2 directory"
check_content "Nested file content" "$SERVER_URL/ds3/dir1/dir2/nested.txt" "Read nested file"

# Edge Cases
test_case "EDGE CASES - EMPTY FILES AND SPECIAL CHARACTERS"

# Create an empty file
temp_file=$(mktemp)
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/empty.txt" "Create empty file"
rm "$temp_file"

check_status 200 "$SERVER_URL/ds3/empty.txt" "Read empty file"

# Create a file with special characters
temp_file=$(create_temp_file "Special chars: !@#\$%^&*()_+-=[]{};\':\",./<>?\\\\")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/special.txt" "Create file with special chars"
rm "$temp_file"

check_content "Special chars" "$SERVER_URL/ds3/special.txt" "Read file with special chars"

# Error Cases
test_case "ERROR HANDLING TESTS"
check_status 404 "$SERVER_URL/invalid/path" "Invalid path (no ds3 prefix)"
check_status 404 "$SERVER_URL/ds3/nonexistent.txt" "Access non-existent file"

# Create a regular file
temp_file=$(create_temp_file "First file")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/file.txt" "Create regular file"
rm "$temp_file"

# Try to create a file inside a file (should fail)
temp_file=$(create_temp_file "Should fail")
check_status 400 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/file.txt/child.txt" "PUT to path where component is file"
rm "$temp_file"

check_status 404 "-X DELETE $SERVER_URL/ds3/nonexistent2.txt" "Delete non-existent file"

# Update Tests
test_case "FILE UPDATE TESTS"

# Create initial file
temp_file=$(create_temp_file "Initial content")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/update_test.txt" "Create initial file"
rm "$temp_file"

check_status 200 "$SERVER_URL/ds3/update_test.txt" "Read initial file"

# Update the file
temp_file=$(create_temp_file "Updated content")
check_status 200 "-X PUT --data-binary @$temp_file $SERVER_URL/ds3/update_test.txt" "Update file"
rm "$temp_file"

check_content "Updated content" "$SERVER_URL/ds3/update_test.txt" "Read updated file"

# Cleanup
test_case "CLEANUP - TESTING DELETE OPERATIONS"
check_status 200 "-X DELETE $SERVER_URL/ds3/file.txt" "Delete file.txt"
check_status 200 "-X DELETE $SERVER_URL/ds3/empty.txt" "Delete empty.txt"
check_status 200 "-X DELETE $SERVER_URL/ds3/special.txt" "Delete special.txt"
check_status 200 "-X DELETE $SERVER_URL/ds3/update_test.txt" "Delete update_test.txt"
check_status 200 "-X DELETE $SERVER_URL/ds3/dir1/dir2/nested.txt" "Delete nested.txt"
check_status 200 "-X DELETE $SERVER_URL/ds3/dir1/dir2" "Delete dir2"
check_status 200 "-X DELETE $SERVER_URL/ds3/dir1" "Delete dir1"

# Verify files and directories were actually deleted
test_case "VERIFICATION - CHECKING DELETES WORKED"
check_status 404 "$SERVER_URL/ds3/file.txt" "Verify file.txt deleted"
check_status 404 "$SERVER_URL/ds3/empty.txt" "Verify empty.txt deleted"
check_status 404 "$SERVER_URL/ds3/special.txt" "Verify special.txt deleted"
check_status 404 "$SERVER_URL/ds3/update_test.txt" "Verify update_test.txt deleted"
check_status 404 "$SERVER_URL/ds3/dir1/dir2/nested.txt" "Verify nested.txt deleted"
check_status 404 "$SERVER_URL/ds3/dir1/dir2" "Verify dir2 deleted"
check_status 404 "$SERVER_URL/ds3/dir1" "Verify dir1 deleted"

# Check root directory is empty or contains only system files
check_content "" "$SERVER_URL/ds3/" "Verify root directory is clean"

echo -e "\n${YELLOW}All tests completed!${NC}"
