# Note: before calling anything from this file
# I2C controller should and must be configured beforehand
.equ I2C_BASE, 0x60013000


# a0: i2c address of slave
# a1: pointer to where data to be written is
# a2: length of data in bytes
.global i2c_write_asm



# t0 must be set to I2C_BASE before calling
# call to synchronise I2C registers
conf_upgate:
    lw  t1, 0x4(t0)
    li  t2, 0x800       # CONF_UPGATE bit
    or  t1, t1, t2
    sw  t1, 0x4(t0)      

    ret

load_address:
    mv  t1, a0          # address from a0
    slli t1, t1, 1      # <<1 and leave as 0 for write command
    sw t1, 0(t0)

    ret

# Doesn't work at the moment
load_data:
    loop:
    lb t2, 0(a1)        # dereference a1
    sb  t2, 0(t0)       # store to FIFO
    addi a1, a1, 4      # increment a1 by word size
    addi a2, a2, -1     
    bne a2, zero, loop  # loop until we've written everything

    ret
    


i2c_write_asm:
    addi sp, sp, -4
    sw  ra, 0(sp)

    li  t0, I2C_BASE

    li  t1, 0x3000      # op_code 6 (RSTART)
    sw  t1, 0x58(t0)    # cmd reg 0

    li  t1, 0x900       # op_code 1 (WRITE) and ack_check_en
    addi t1, t1, 1      # address is always going to be 1 byte
    add t1, t1, a2      # get the number of bytes to be written from a2
    sw  t1, 0x5C(t0)    # cmd reg 1

    li  t1, 0x1000      # op_code 4 (END)
    sw  t1, 0x60(t0)    # cmd reg 2

    addi t0, t0, 0x1C   # set t0 to FIFO reg
    call load_address   # load slave address
    call load_data      # load data to be written

    li  t0, I2C_BASE
    call conf_upgate


    lw  t1, 0x4(t0)
    ori t1, t1, 0x20    # I2C_TRANS_STAT_BIT
    sw  t1, 0x4(t0)


    lw  ra, 0(sp)
    addi sp, sp,4

    ret




    





    




