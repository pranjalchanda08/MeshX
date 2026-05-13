## Unit test Command Table

> Note: All the data to be sent or received shall happen only if the element is provisioned and a valid publish address is provided during provisioning

**Module** : `Switch Relay Client`
**Module ID** : `0x00`

| Command                 | Cmd ID | UT Command                  | Description                               | Status |
| ----------------------- | ------ | --------------------------- | ----------------------------------------- | ------ |
| RELAY_CLI_CMD_GET       | 0x00   | ut 0 0 1  `[el_id]`         | Send Relay ONOFF GET msg to element_id    | PASS   |
| RELAY_CLI_CMD_SET       | 0x01   | ut 0 1 1  `[el_id]`         | Send Relay ONOFF SET msg to element_id    | PASS   |
| RELAY_CLI_CMD_SET_UNACK | 0x02   | ut 0 2 1  `[el_id]`         | Send Relay ONOFF SET UNACK msg to element_id | PASS   |
| RELAY_CLI_CMD_CONFIG    | 0x03   | ut 0 3 3  `[el_id]` `[pub_addr]` `[app_id]` | Configure publication address and app key | PASS   |


**Module** : `Light CWWW Client`
**Module ID** : `0x01`

| Command                               | Cmd ID | UT Command                                               | Description                                                     | Status |
| ------------------------------------- | ------ | -------------------------------------------------------- | --------------------------------------------------------------- | ------ |
| CWWW_CLI_UT_CMD_ONOFF_GET             | 0x00   | ut 1 0 1  `[el_id]`                                      | Send CWWW ONOFF GET msg to element_id                           | PASS   |
| CWWW_CLI_UT_CMD_ONOFF_SET             | 0x01   | ut 1 1 1  `[el_id]`                                      | Send CWWW ONOFF SET msg to element_id                           | PASS   |
| CWWW_CLI_UT_CMD_ONOFF_SET_UNACK       | 0x02   | ut 1 2 1  `[el_id]`                                      | Send CWWW ONOFF SET UNACK msg to element_id                     | PASS   |
| CWWW_CLI_UT_CMD_CONFIG                | 0x03   | ut 1 3 3  `[el_id]` `[pub_addr]` `[app_id]`              | Configure publication address and app key                       | PASS   |
| CWWW_CLI_UT_CMD_CTL_SET               | 0x04   | ut 1 4 4  `[el_id]` `[temp]` `[brightness]` `[delta_uv]` | Send CWWW CTL SET Command to element_id                         | PASS   |
| CWWW_CLI_UT_CMD_CTL_SET_UNACK         | 0x05   | ut 1 5 4  `[el_id]` `[temp]` `[brightness]` `[delta_uv]` | Send CWWW CTL SET UNACK Command to element_id                   | PASS   |
| CWWW_CLI_UT_CMD_LIGHTNESS_SET         | 0x06   | ut 1 6 2  `[el_id]` `[brigntness]`                       | Send CWWW LIGHTNESS SET Command to element_id                   | PASS   |
| CWWW_CLI_UT_CMD_LIGHTNESS_SET_UNACK   | 0x07   | ut 1 7 2  `[el_id]` `[brigntness]`                       | Send CWWW LIGHTNESS SET UNACK Command to element_id             | PASS   |
| CWWW_CLI_UT_CMD_TEMPERATURE_SET       | 0x08   | ut 1 8 2  `[el_id]` `[temperature]`                      | Send CWWW TEMPERATURE SET Command to element_id                 | PASS   |
| CWWW_CLI_UT_CMD_TEMPERATURE_SET_UNACK | 0x09   | ut 1 9 2  `[el_id]` `[temperature]`                      | Send CWWW TEMPERATURE SET UNACK Command to element_id           | PASS   |
| CWWW_CLI_UT_CMD_DELTA_UV_SET          | 0x0A   | ut 1 10 2 `[el_id]` `[delta_uv]`                         | Send CWWW DELTA UV SET Command to element_id                    | PASS   |
| CWWW_CLI_UT_CMD_DELTA_UV_SET_UNACK    | 0x0B   | ut 1 11 2 `[el_id]` `[delta_uv]`                         | Send CWWW DELTA UV SET UNACK Command to element_id              | PASS   |
| CWWW_CLI_UT_CMD_TEMP_RANGE_SET        | 0x0C   | ut 1 12 3 `[el_id]` `[min]` `[max]`                      | Send CWWW TEMPERATURE RANGE SET for target publish server       | FAIL   |
| CWWW_CLI_UT_CMD_TEMP_RANGE_SET_UNACK  | 0x0D   | ut 1 13 3 `[el_id]` `[min]` `[max]`                      | Send CWWW TEMPERATURE RANGE SET UNACK for target publish server | FAIL   |

**Module** : `OS Timer`
**Module ID** : `0x02`

| Command                     | Cmd ID | UT Command                        | Description                             | Status |
| --------------------------- | ------ | --------------------------------- | --------------------------------------- | ------ |
| OS_TIMER_CLI_CMD_CREATE     | 0x00   | ut 2 0 2 `[period_ms]` `[reload]` | Initiallise Unit Test OS Timer Instance | PASS   |
| OS_TIMER_CLI_CMD_ARM        | 0x01   | ut 2 1 0                          | Arm Unit Test OS Timer Instance         | PASS   |
| OS_TIMER_CLI_CMD_REARM      | 0x02   | ut 2 2 0                          | Re-Arm Unit Test OS Timer Instance      | PASS   |
| OS_TIMER_CLI_CMD_DISARM     | 0x03   | ut 2 3 0                          | Disarm Unit Test OS Timer Instance      | PASS   |
| OS_TIMER_CLI_CMD_DELETE     | 0x04   | ut 2 4 0                          | Delete Unit Test OS Timer Instance      | PASS   |
| OS_TIMER_CLI_CMD_PERIOD_SET | 0x05   | ut 2 5 1 `[period_ms]`            | Period Set Unit Test OS Timer Instance  | PASS   |

**Module** : `MeshX NVS`
**Module ID** : `0x03`

| Command                  | Cmd ID | UT Command                    | Description                            | Status |
| ------------------------ | ------ | ----------------------------- | -------------------------------------- | ------ |
| MESHX_NVS_CLI_CMD_OPEN   | 0x00   | ut 3 0 0                      | MeshX NVS nampespace open              | PASS   |
| MESHX_NVS_CLI_CMD_SET    | 0x01   | ut 3 1 1 `[arm commit timer]` | MeshX NVS set blob value               | PASS   |
| MESHX_NVS_CLI_CMD_GET    | 0x02   | ut 3 2 0                      | MeshX NVS get blob value               | PASS   |
| MESHX_NVS_CLI_CMD_COMMIT | 0x03   | ut 3 3 0                      | MeshX NVS commit the MeshX NVS changes | PASS   |
| MESHX_NVS_CLI_CMD_REMOVE | 0x04   | ut 3 4 0                      | MeshX NVS remove MeshX UT key          | PASS   |
| MESHX_NVS_CLI_CMD_ERASE  | 0x05   | ut 3 5 0                      | MeshX NVS erase all from MeshX UT key  | PASS   |
| MESHX_NVS_CLI_CMD_CLOSE  | 0x06   | ut 3 6 0                      | MeshX NVS close driver                 | PASS   |

**Module** : `Switch Relay Server`
**Module ID** : `0x04`

| Command                   | Cmd ID | UT Command                  | Description                        | Status |
| ------------------------- | ------ | --------------------------- | ---------------------------------- | ------ |
| RELAY_SRV_CMD_CHECK_STATE | 0x01   | ut 4 1 2 `[el_id]` `[state]` | Verify local Relay state (0 or 1) | PASS   |

**Module** : `Light CWWW Server`
**Module ID** : `0x05`

| Command                   | Cmd ID | UT Command                            | Description                            | Status |
| ------------------------- | ------ | ------------------------------------- | -------------------------------------- | ------ |
| CWWW_SRV_CMD_CHECK_ONOFF  | 0x01   | ut 5 1 2 `[el_id]` `[state]`          | Verify local CWWW OnOff state (0 or 1) | PASS   |
| CWWW_SRV_CMD_CHECK_CTL    | 0x02   | ut 5 2 3 `[el_id]` `[light]` `[temp]` | Verify local CWWW CTL state            | PASS   |

**Module** : `GPIO Unit Tests`
**Module ID** : `0x10`

GPIO unit tests validate all GPIO API functions with valid and invalid inputs,
error conditions, recovery strategies, and function-based API operations.

**Validates: Requirements 10.1-10.4**

| Command                                    | Cmd ID | UT Command      | Description                                         | Status |
| ------------------------------------------ | ------ | --------------- | --------------------------------------------------- | ------ |
| GPIO_UT_CMD_RUN_ALL                        | 0x00   | ut 17 0 0       | Run all GPIO unit tests                             | PASS   |
| GPIO_UT_CMD_INIT                           | 0x01   | ut 17 1 0       | Test GPIO initialization                            | PASS   |
| GPIO_UT_CMD_DEINIT                         | 0x02   | ut 17 2 0       | Test GPIO deinitialization                          | PASS   |
| GPIO_UT_CMD_REINIT                         | 0x03   | ut 17 3 0       | Test GPIO re-initialization                         | PASS   |
| GPIO_UT_CMD_SET_LEVEL_VALID                | 0x04   | ut 17 4 0       | Test set_level with valid inputs                    | PASS   |
| GPIO_UT_CMD_SET_LEVEL_INVALID              | 0x05   | ut 17 5 0       | Test set_level with invalid inputs                  | PASS   |
| GPIO_UT_CMD_SET_LEVEL_ALL_MODES            | 0x06   | ut 17 6 0       | Test set_level on all GPIO modes                    | PASS   |
| GPIO_UT_CMD_GET_LEVEL_VALID                | 0x07   | ut 17 7 0       | Test get_level with valid inputs                    | PASS   |
| GPIO_UT_CMD_GET_LEVEL_INVALID              | 0x08   | ut 17 8 0       | Test get_level with invalid inputs                  | PASS   |
| GPIO_UT_CMD_TOGGLE_VALID                   | 0x09   | ut 17 9 0       | Test toggle operation                               | PASS   |
| GPIO_UT_CMD_TOGGLE_INVALID                 | 0x0A   | ut 17 10 0      | Test toggle on invalid mode                         | PASS   |
| GPIO_UT_CMD_INTERRUPT_REGISTER             | 0x0B   | ut 17 11 0      | Test interrupt registration                         | PASS   |
| GPIO_UT_CMD_INTERRUPT_REGISTER_INVALID     | 0x0C   | ut 17 12 0      | Test interrupt registration with invalid inputs     | PASS   |
| GPIO_UT_CMD_INTERRUPT_ENABLE_DISABLE       | 0x0D   | ut 17 13 0      | Test interrupt enable/disable                       | PASS   |
| GPIO_UT_CMD_ALL_INTERRUPT_TYPES            | 0x0E   | ut 17 14 0      | Test all interrupt trigger types                    | PASS   |
| GPIO_UT_CMD_EXECUTE_FUNCTION_SET_LEVEL     | 0x0F   | ut 17 15 0      | Test execute_function SET_LEVEL                     | PASS   |
| GPIO_UT_CMD_EXECUTE_FUNCTION_TOGGLE        | 0x10   | ut 17 16 0      | Test execute_function TOGGLE                        | PASS   |
| GPIO_UT_CMD_EXECUTE_FUNCTION_PWM_DUTY      | 0x11   | ut 17 17 0      | Test execute_function SET_PWM_DUTY                  | PASS   |
| GPIO_UT_CMD_EXECUTE_FUNCTION_PWM_FREQ      | 0x12   | ut 17 18 0      | Test execute_function SET_PWM_FREQUENCY             | PASS   |
| GPIO_UT_CMD_EXECUTE_FUNCTION_MULTI_ARGS    | 0x13   | ut 17 19 0      | Test execute_function with multiple arguments       | PASS   |
| GPIO_UT_CMD_HOSTED_MODE_SET_GET            | 0x14   | ut 17 20 0      | Test hosted mode set/get                            | PASS   |
| GPIO_UT_CMD_HOSTED_MODE_OPERATIONS         | 0x15   | ut 17 21 0      | Test operations in hosted mode                      | PASS   |
| GPIO_UT_CMD_ERROR_RECOVERY_INVALID_INPUT   | 0x16   | ut 17 22 0      | Test error recovery after invalid input             | PASS   |
| GPIO_UT_CMD_ERROR_RECOVERY_MODE_MISMATCH   | 0x17   | ut 17 23 0      | Test error recovery after mode mismatch             | PASS   |

**Module** : `GPIO Property Tests`
**Module ID** : `0x12`

GPIO property tests validate Property 2: Mode-Aware GPIO Operation Validity.

**Validates: Requirements 2.1-2.5, 2.8-2.10, 8.1-8.3, 8.9-8.10**

| Command                                | Cmd ID | UT Command     | Description                                    | Status |
| -------------------------------------- | ------ | -------------- | ---------------------------------------------- | ------ |
| GPIO_PROPERTY_CMD_RUN_ALL              | 0x00   | ut 18 0 0      | Run all GPIO property tests                    | PASS   |
| GPIO_PROPERTY_CMD_MODE_OPERATIONS      | 0x01   | ut 18 1 0      | Property 2.1: Mode-specific operations         | PASS   |
| GPIO_PROPERTY_CMD_ERROR_CODES          | 0x02   | ut 18 2 0      | Property 2.2: Appropriate error codes          | PASS   |
| GPIO_PROPERTY_CMD_OPERATION_ISOLATION  | 0x03   | ut 18 3 0      | Property 2.3: Operation isolation              | PASS   |
| GPIO_PROPERTY_CMD_STATE_TRACKING       | 0x04   | ut 18 4 0      | Property 2.4: Consistent state tracking        | PASS   |
| GPIO_PROPERTY_CMD_INIT_DEINIT          | 0x05   | ut 18 5 0      | Property 2.5: Initialization/deinitialization  | PASS   |

**Module** : `PWM Property Tests`
**Module ID** : `0x14`

PWM property tests validate Property 4: PWM Subsystem Correctness.

**Validates: Requirements 4.1-4.10**

| Command                              | Cmd ID | UT Command     | Description                                    | Status |
| ------------------------------------ | ------ | -------------- | ---------------------------------------------- | ------ |
| PWM_PROPERTY_CMD_RUN_ALL             | 0x00   | ut 20 0 0      | Run all PWM property tests                     | PASS   |
| PWM_PROPERTY_CMD_INIT                | 0x01   | ut 20 1 0      | Property 4.1: PWM initialization               | PASS   |
| PWM_PROPERTY_CMD_START_STOP          | 0x02   | ut 20 2 0      | Property 4.2: PWM start/stop operations        | PASS   |
| PWM_PROPERTY_CMD_DUTY_CYCLE          | 0x03   | ut 20 3 0      | Property 4.3: Duty cycle accuracy              | PASS   |
| PWM_PROPERTY_CMD_FREQUENCY           | 0x04   | ut 20 4 0      | Property 4.4: Frequency accuracy               | PASS   |
| PWM_PROPERTY_CMD_PARAM_VALIDATION    | 0x05   | ut 20 5 0      | Property 4.5: Parameter validation             | PASS   |
| PWM_PROPERTY_CMD_CHANNEL_ALLOC       | 0x06   | ut 20 6 0      | Property 4.6: Channel allocation               | PASS   |
| PWM_PROPERTY_CMD_STATE_MAINTENANCE   | 0x07   | ut 20 7 0      | Property 4.7: State maintenance                | PASS   |
| PWM_PROPERTY_CMD_DEINIT_CLEANUP      | 0x08   | ut 20 8 0      | Property 4.8: Deinitialization cleanup         | PASS   |

> Note: GPIO and PWM property tests share the same module ID (0x10) with GPIO unit tests.
> The command handler distinguishes between property tests and unit tests based on the
> registered callback function.
