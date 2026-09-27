#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work_dir="${project_root}/.work/native"
image_name="endless-sky-dos-native-reference:local"

mkdir -p "${work_dir}"
git -C "${project_root}" rev-parse HEAD > "${work_dir}/source-commit.txt"
docker build -f "${project_root}/dos/Dockerfile.native" -t "${image_name}" "${project_root}/dos"
docker image inspect --format '{{.Id}}' "${image_name}" > "${work_dir}/image-id.txt"

docker run --rm \
  --user "$(id -u):$(id -g)" \
  --env HOME=/tmp \
  --network none \
  --mount "type=bind,src=${project_root},dst=/src,readonly" \
  --mount "type=bind,src=${work_dir},dst=/work" \
  --workdir /src \
  "${image_name}" \
  bash -euo pipefail -c '
    dpkg-query -W -f="\${Package}=\${Version}\n" > /work/packages.txt
    cmake -S /src -B /work/build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DES_USE_VCPKG=OFF
    timeout 360 cmake --build /work/build --parallel 2

    rm -rf /work/parse-config /work/test-config
    mkdir -p /work/parse-config /work/test-config
    cp -a /src/tests/integration/config/. /work/parse-config/
    cp -a /src/tests/integration/config/. /work/test-config/

    # The wrapper only prints its marker after the upstream landing test returns.
    cat > /work/test-config/plugins/integration-tests/data/tests/native_reference.txt <<EOF
test "DOS Native Reference Landing"
    status active
    sequence
        call "Landing in a system with multiple planets"
        debug "DOS_NATIVE_REFERENCE_PASS"
EOF

    if /usr/bin/time -v -o /work/parse.time \
      timeout 90 /work/build/endless-sky --resources /src --config /work/parse-config \
      --parse-save --tq-threads 1 > /work/parse.log 2>&1; then
      tail -n 10 /work/parse.log
    else
      cat /work/parse.log
      exit 1
    fi

    # main.cpp accepts --rngseed (although its help text says --rng-seed).
    if /usr/bin/time -v -o /work/integration.time \
      timeout 180 /work/build/endless-sky --resources /src --config /work/test-config \
      --test "DOS Native Reference Landing" --rngseed 1 \
      --tq-threads 1 > /work/integration.log 2>&1; then
      if ! grep -Fxq DOS_NATIVE_REFERENCE_PASS /work/integration.log; then
        cat /work/integration.log
        printf "The landing test returned without its completion marker.\n" >&2
        exit 1
      fi
      tail -n 10 /work/integration.log
    else
      cat /work/integration.log
      exit 1
    fi

    printf "Native reference reports: /work/parse.time and /work/integration.time\n"
  '

printf 'Reports and logs: %s\n' "${work_dir}"
