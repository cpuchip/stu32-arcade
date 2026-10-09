(module
  (func $deep (export "deep") (param i32) (result i32)
    local.get 0
    i32.eqz
    if (result i32)
      i32.const 0
    else
      local.get 0
      i32.const 1
      i32.sub
      call $deep
      i32.const 1
      i32.add
    end)
  (func (export "add") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add))
