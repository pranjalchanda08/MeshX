## Unit test Command Table

> Note: All the data to be sent or received shall happen only if the element is provisioned and a valid publish address is provided during provisioning

| Element Type        | Module ID | Command ID                            |        | UT Command                                               | Description                                                     | Status |
| ------------------- | --------- | ------------------------------------- | ------ | -------------------------------------------------------- | --------------------------------------------------------------- | ------ |
| Switch Relay Client | 0x00      | RELAY_CLI_CMD_GET                     | (0x00) | ut 0 0 1  `[el_id]`                                      | Send Relay ONOFF GET msg to element_id                          | `FAIL` |
|                     |           | RELAY_CLI_CMD_SET                     | (0x01) | ut 0 1 1  `[el_id]`                                      | Send Relay ONOFF SET msg to element_id                          | PASS   |
|                     |           | RELAY_CLI_CMD_SET_UNACK               | (0x02) | ut 0 2 1  `[el_id]`                                      | Send Relay ONOFF SET UNACK msg to element_id                    | PASS   |
| Light CWWW Client   | 0x01      | CWWW_CLI_UT_CMD_ONOFF_GET             | (0x00) | ut 1 0 1  `[el_id]`                                      | Send CWWW ONOFF GET msg to element_id                           | `FAIL` |
|                     |           | CWWW_CLI_UT_CMD_ONOFF_SET             | (0x01) | ut 1 1 1  `[el_id]`                                      | Send CWWW ONOFF SET msg to element_id                           | PASS   |
|                     |           | CWWW_CLI_UT_CMD_ONOFF_SET_UNACK       | (0x02) | ut 1 2 1  `[el_id]`                                      | Send CWWW ONOFF SET UNACK msg to element_id                     | PASS   |
|                     |           | CWWW_CLI_UT_CMD_CTL_GET               | (0x03) | ut 1 3 1  `[el_id]`                                      | Send CWWW CTL GET Command to element_id                         | `FAIL` |
|                     |           | CWWW_CLI_UT_CMD_CTL_SET               | (0x04) | ut 1 4 4  `[el_id]` `[temp]` `[brightness]` `[delta_uv]` | Send CWWW CTL SET Command to element_id                         | PASS   |
|                     |           | CWWW_CLI_UT_CMD_CTL_SET_UNACK         | (0x05) | ut 1 5 4  `[el_id]` `[temp]` `[brightness]` `[delta_uv]` | Send CWWW CTL SET UNACK Command to element_id                   | PASS   |
|                     |           | CWWW_CLI_UT_CMD_LIGHTNESS_SET         | (0x06) | ut 1 6 2  `[el_id]` `[brigntness]`                       | Send CWWW LIGHTNESS SET Command to element_id                   | PASS   |
|                     |           | CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK   | (0x07) | ut 1 7 2  `[el_id]` `[brigntness]`                       | Send CWWW LIGHTNESS SET UNACK Command to element_id             | PASS   |
|                     |           | CWWW_CLI_UT_CMD_TEMPERATURE_SET       | (0x08) | ut 1 8 2  `[el_id]` `[temperature]`                      | Send CWWW TEMPERATURE SET Command to element_id                 | PASS   |
|                     |           | CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK | (0x09) | ut 1 9 2  `[el_id]` `[temperature]`                      | Send CWWW TEMPERATURE SET UNACK Command to element_id           | PASS   |
|                     |           | CWWW_CLI_UT_CMD_DELTA_UV_SET          | (0x0A) | ut 1 10 2 `[el_id]` `[delta_uv]`                         | Send CWWW DELTA UV SET Command to element_id                    | PASS   |
|                     |           | CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK    | (0x0B) | ut 1 11 2 `[el_id]` `[delta_uv]`                         | Send CWWW DELTA UV SET UNACK Command to element_id              | PASS   |
|                     |           | CWWW_CLI_UT_CMD_TEMOP_RANGE_SET       | (0x0C) | ut 1 12 3 `[el_id]` `[min]` `[max]`                      | Send CWWW TEMPERATURE RANGE SET for target publish server       | PASS   |
|                     |           | CWWW_CLI_UT_CMD_TEMOP_RANGE_SET_UNACK | (0x0D) | ut 1 13 3 `[el_id]` `[min]` `[max]`                      | Send CWWW TEMPERATURE RANGE SET UNACK for target publish server | PASS   |
| OS Timer            | 0x02      | OS_TIMER_CLI_CMD_CREATE               | (0x00) | ut 2 0 2 `[period_ms]` `[reload]`                        | Initiallise Unit Test OS Timer Instance                         | PASS   |
|                     |           | OS_TIMER_CLI_CMD_ARM                  | (0x01) | ut 2 1 0                                                 | Arm Unit Test OS Timer Instance                                 | PASS   |
|                     |           | OS_TIMER_CLI_CMD_DISARM               | (0x02) | ut 2 2 0                                                 | Disarm Unit Test OS Timer Instance                              | PASS   |
|                     |           | OS_TIMER_CLI_CMD_DELETE               | (0x03) | ut 2 3 0                                                 | Delete Unit Test OS Timer Instance                              | PASS   |
