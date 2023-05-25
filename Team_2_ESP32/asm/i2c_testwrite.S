.equ I2C_BASE, 0x60013000

.global i2c_write

# t0 MUST ALREADY BE SET TO I2C_BASE BEFORE CALLING
conf_upgate:
    lw t1, 0x4(t0)
    li t2, 0x800            # CONF_UPGATE bit
    or t1, t1, t2           
    sw t1, 0x4(t0)

    ret

# t0 must be set to I2C_BASE before calling
load_rstart_cmd:
    li t1, 0x3000           # sets op_code to 6 (RSTART)
    sw t1, 0x58(t0)         # everything else is ignored/unnecessary

    ret
    
load_write_cmd:
    li t1, 0x900            # set op_code to 1 (WRITE) and ack_check_en bit
    addi t1, t1, 2          # number of bytes to be written TODO: take this number from arg 2
    sw t1, 0x5C(t0)
    
    ret

load_end_cmd:
    li t1, 4096             # set op_code to 2 (STOP)
    sw t1, 0x60(t0)         # everything else is ignored for this

    ret
    
load_address:
    mv t1, a0               # first argument is slave address
    slli t1, t1, 1          # << 1 and write bit is 0 so leave it as is
    sw t1, (t0)

    ret


load_testdata:
    li t1, 0xaf             # just using a known value here
    sw t1, (t0)

    ret

i2c_write:
    addi sp, sp, -4          
    sw  ra, 0(sp)           # store original return address on stack

    li t0, I2C_BASE


    # load command registers
    call load_rstart_cmd
    call load_write_cmd
    call load_end_cmd


    # load data to be written into TX FIFO
    addi t0, t0, 0x1C       # set t0 to FIFO reg, required(?)
    call load_address
    call load_testdata

    li t0, I2C_BASE         # fix back to I2C_BASE

    call conf_upgate

    lw t1, 0x4(t0)
    ori t1, t1, 0x20        # set I2C_TRANS_START bit
    sw t1, 0x4(t0)



    lw ra, 0(sp)            # load original return address back
    addi sp, sp, 4           
                            # a0 should have kept initial passed value of address

    ret
