#!/bin/bash

# Add local user
# Either use the LOCAL_USER_ID if passed in at runtime or
# fallback

USER_ID=${LOCAL_USER_ID:-9001}
GROUP_ID=${LOCAL_GROUP_ID:-1000}

echo "Starting with UID : $USER_ID"
groupadd -g $GROUP_ID -o parsa
useradd --shell /bin/bash -u "$USER_ID" -o -g "$GROUP_ID" -c "" -m user
export HOME=/home/user

exec /usr/local/bin/gosu user "$@"
