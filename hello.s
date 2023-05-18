
# MIPS assembly generated using lscc
_start:
    addiu   $sp, $sp, -24
    sw      $fp, 20($sp)
    sw      $31, 16($sp)
    move    $fp, $sp
    sw      $4, 24($fp)
    sw      $5, 28($fp)
    sw      $6, 32($fp)
    sw      $7, 36($fp)
  fnc__start_code:
    move    $8, $0
    sw      $8, 8($fp)
    nop
    lw     $8, 8($fp)
    nop
    move    $2, $8
    j       fnc__start_return
    nop
  fnc__start_return:
    move    $sp, $fp
    lw      $31, 16($sp)
    lw      $fp, 20($sp)
    addiu   $sp, $sp, 24
    j       $31
    nop

