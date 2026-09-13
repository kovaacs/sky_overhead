variable "RELEASE_VERSION" {
  default = "v0.0.0"
}

group "default" {
  targets = ["firmware", "release"]
}

target "common" {
  context = "."
  dockerfile = "Dockerfile"
  platforms = ["linux/arm64"]
}

target "tests" {
  inherits = ["common"]
  target = "tests"
  output = ["type=cacheonly"]
}

target "firmware" {
  inherits = ["common"]
  target = "firmware"
  output = ["type=local,dest=.build/firmware"]
}

target "release" {
  inherits = ["common"]
  target = "release"
  args = {
    RELEASE_VERSION = RELEASE_VERSION
  }
  output = ["type=local,dest=.build/release"]
}
