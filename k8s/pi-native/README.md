# Native ARM64 Builder

This directory started as the old Raspberry Pi helper, but it now serves as the self-contained native ARM64 build lane for AtlasNet, including AWS CodeBuild.

The AWS CodeBuild path builds these ARM64 images:

- `watchdog`
- `proxy`
- `cartograph`
- `sandbox-server`

Those are the same image names the current `k8s/k3s` deploy flow expects.

## Files

- `Makefile` - local and CI build/push entrypoints
- `buildspec.yml` - AWS CodeBuild buildspec; point the CodeBuild project at `k8s/pi-native/buildspec.yml`
- `run-codebuild.sh` - shell entrypoint used by the buildspec and by optional local native runs
- `codebuild.env.example` - checked-in non-secret environment variable template
- `codebuild.env` - optional untracked local file loaded automatically by `run-codebuild.sh` when present

## What This Replaces

This path replaces the old "build on a Pi and push arm64 tags" workflow. The directory name is still `pi-native` so existing links do not break, but the intended target is now any native ARM64 host, especially AWS CodeBuild on Graviton.

## One-Time Repo Prep

1. Commit and push the files in this directory so AWS CodeBuild can read them from your repo.
2. Decide the image tags you want to use:
   - `ATLASNET_IMAGE_TAG=latest`
   - `ATLASNET_IMAGE_TAG_AMD64=latest-amd64`
   - `ATLASNET_IMAGE_TAG_ARM64=latest-arm64`
3. Keep using your existing amd64 build path locally or in a separate x86 build job.

## One-Time AWS Prep

### 1. Make Sure CodeBuild Can Reach Your Repo

AWS CodeBuild needs a remote source. The easiest option is a GitHub repo connected through AWS CodeConnections.

If your repo is not on GitHub yet, put it somewhere CodeBuild can clone first. CodeBuild cannot read the local copy sitting on your workstation.

### 2. Create a Docker Hub Access Token

In Docker Hub:

1. Open `Account Settings -> Personal access tokens`.
2. Create a token for CodeBuild.
3. Copy the token value somewhere safe. You will only need it once for AWS setup.

### 3. Store the Docker Hub Token in AWS Secrets Manager

In AWS:

1. Open `Secrets Manager`.
2. Create a new secret.
3. Store the Docker Hub token as a plain text secret.
4. Give it a name like `atlasnet/dockerhub/token`.

You can keep `DOCKERHUB_USERNAME` as a normal CodeBuild environment variable. Only the token needs to be secret.

### 4. Create the CodeBuild Project

In the AWS console, open `CodeBuild -> Create build project`.

Use these settings:

1. Project name:
   `atlasnet-arm64`
2. Source provider:
   your GitHub repo through `CodeConnections`
3. Source version:
   the branch you want to publish from, usually `main`
4. Provisioning model:
   `On-demand`
5. Compute:
   `EC2`
6. Running mode:
   `Instance`
7. Environment image:
   `Managed image`
8. Compute size:
   start with at least `8 vCPU / 16 GiB` if available, or the closest larger size you see
9. Image:
   keep the default ARM EC2 managed image AWS selects for the instance host

In instance mode, AWS currently provisions an ARM EC2 host image rather than the Ubuntu container image you may see in container mode. The helper in this directory now supports both Ubuntu `apt` and Amazon Linux `dnf`, so instance mode is fine.

For the rest:

1. Service role:
   let CodeBuild create a new role unless you already manage your own
2. Buildspec:
   `Use a buildspec file`
3. Buildspec name:
   `k8s/pi-native/buildspec.yml`
4. Artifacts:
   `No artifacts`
5. Logs:
   leave CloudWatch Logs enabled

### 5. Add CodeBuild Environment Variables

In the CodeBuild project settings, add these variables:

Plaintext variables:

- `DOCKERHUB_NAMESPACE=yourdockerhubuser`
- `DOCKERHUB_USERNAME=yourdockerhubuser`
- `ATLASNET_IMAGE_TAG=latest`
- `ATLASNET_IMAGE_TAG_AMD64=latest-amd64`
- `ATLASNET_IMAGE_TAG_ARM64=latest-arm64`
- `CMAKE_BUILD_TYPE=Release`
- `JOBS=4`

Secret variable:

- `DOCKERHUB_TOKEN`
  Point this at the Secrets Manager secret you created earlier.

If you want custom image names, you can also set:

- `ATLASNET_WATCHDOG_IMAGE`
- `ATLASNET_PROXY_IMAGE`
- `ATLASNET_CARTOGRAPH_IMAGE`
- `ATLASNET_SANDBOX_SERVER_IMAGE`

If you do not set those, the Makefile defaults to:

- `${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG_ARM64}`
- `${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG_ARM64}`
- `${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG_ARM64}`
- `${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG_ARM64}`

### 6. Start the First ARM64 Build

In CodeBuild:

1. Save the project.
2. Click `Start build`.
3. Watch the logs.

The buildspec will:

1. Install the minimal host toolchain needed by this native ARM build lane.
   The host only gets compilers, build tools, SWIG, Node headers, Docker, and kernel headers.
2. Bootstrap the pinned `vcpkg` release used by the Dockerfiles (`2026.01.16` by default).
3. Configure an ARM64 build tree.
4. Run:
   - `AtlasnetDockerBuild_Fast`
   - `SandboxServerDockerBuild`
5. Log in to Docker Hub.
6. Push the ARM64-tagged images.

### 7. Verify Docker Hub Output

After the build finishes, confirm these exist in Docker Hub:

- `yourdockerhubuser/watchdog:latest-arm64`
- `yourdockerhubuser/proxy:latest-arm64`
- `yourdockerhubuser/cartograph:latest-arm64`
- `yourdockerhubuser/sandbox-server:latest-arm64`

## After ARM64 Is Working

Your existing deploy path wants a multi-arch manifest tag for mixed clusters.

So after you also have the amd64 images published, run this from your usual machine:

```bash
make -C k8s/k3s atlasnet-merge-manifests
```

That combines:

- `*:latest-amd64`
- `*:latest-arm64`

into:

- `*:latest`

Then your normal deploy path can keep using:

```bash
make -C k8s/k3s atlasnet-deploy
```

## Optional Local Smoke Test

If you ever have a local ARM64 machine and want to test the same flow before using AWS:

```bash
cp k8s/pi-native/codebuild.env.example k8s/pi-native/codebuild.env
$EDITOR k8s/pi-native/codebuild.env
bash k8s/pi-native/run-codebuild.sh
```

`run-codebuild.sh` automatically loads `k8s/pi-native/codebuild.env` if it exists. Keep secrets like `DOCKERHUB_TOKEN` out of that file unless you intentionally want them stored locally.

## Troubleshooting

- `docker: Cannot connect to the Docker daemon`
  Enable `Privileged mode` in the CodeBuild project.
- `apt-get: command not found`
  The project is probably using Amazon Linux instance mode. That is supported now; make sure the latest `k8s/pi-native/Makefile` is committed on the branch CodeBuild is using.
- The log stops around an early Boost package and never shows the actual failure
  The Makefile now prints the tail of `build-arm64-codebuild/vcpkg-manifest-install.log` plus nearby CMake logs so CodeBuild exposes the real error instead of the first 240 lines.
- Unsure whether a dependency belongs in the OS packages or in `vcpkg`
  In this path, the host package manager only installs build tools and headers. AtlasNet C/C++ libraries are expected to come from `vcpkg` during CMake configure.
- Build runs out of memory or times out
  Increase the ARM64 compute size.
- Docker Hub push fails with auth errors
  Re-check `DOCKERHUB_USERNAME` and the `DOCKERHUB_TOKEN` secret.
- ARM64 images push fine but deploy still pulls the wrong thing
  You still need the multi-arch merge step after the amd64 images exist.

## CLI Trigger

Once the project exists, you can start it again from a terminal:

```bash
aws codebuild start-build --project-name atlasnet-arm64
```
