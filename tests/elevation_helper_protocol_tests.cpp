#include "../src/adapters/windows/window_management/elevation_helper_protocol.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message) {
  if (!condition) {
    ++failures;
    printf("FAIL: %s\n", message);
  }
}

static reach_elevation_helper_request valid_request(void) {
  reach_elevation_helper_request request = {};
  request.version = reach_elevation_helper_protocol_version();
  request.command = REACH_ELEVATION_HELPER_COMMAND_ACTIVATE;
  request.window = 123;
  request.split_mode = REACH_SPLIT_LEFT;
  return request;
}

int main(void) {
  expect_true(reach_elevation_helper_protocol_version() == 1,
              "protocol version is stable");

  reach_elevation_helper_request request = valid_request();
  expect_true(reach_elevation_helper_request_valid(&request),
              "valid activate request is accepted");

  request = valid_request();
  request.version = 999;
  expect_true(!reach_elevation_helper_request_valid(&request),
              "bad version is rejected");

  request = valid_request();
  request.window = 0;
  expect_true(!reach_elevation_helper_request_valid(&request),
              "missing window is rejected");

  request = valid_request();
  request.command = 999;
  expect_true(!reach_elevation_helper_request_valid(&request),
              "unknown command is rejected");

  request = valid_request();
  request.command = REACH_ELEVATION_HELPER_COMMAND_SNAP;
  request.split_mode = REACH_SPLIT_FULL;
  expect_true(reach_elevation_helper_request_valid(&request),
              "full split snap request is accepted");

  request = valid_request();
  request.command = REACH_ELEVATION_HELPER_COMMAND_SNAP;
  request.split_mode = 100;
  expect_true(!reach_elevation_helper_request_valid(&request),
              "bad split mode is rejected");

  return failures == 0 ? 0 : 1;
}
