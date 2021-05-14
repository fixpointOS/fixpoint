(module
  (type (;0;) (func (param i32 i32)))
  (type (;1;) (func (param i32 i32 i32)))
  (type (;2;) (func (param i32 i32) (result i32)))
  (type (;3;) (func))
  (type (;4;) (func (param i32)))
  (type (;5;) (func (param i32 i32 i32 i32)))
  (import "env" "attach_input" (func $attach_input (type 0)))
  (import "env" "attach_output" (func $attach_output (type 1)))
  (import "env" "get_int" (func $get_int (type 2)))
  (import "env" "store_int" (func $store_int (type 0)))
  (import "env" "move_lazy_input" (func $move_lazy_input (type 1)))
  (import "env" "attach_output_child" (func $attach_output_child (type 5)))
  (import "env" "set_encode" (func $set_encode (type 0)))
  (import "env" "add_path" (func $add_path (type 0)))
  (func $fib (param $n i32)
    (if 
      (i32.eqz (get_local $n))
      (then (call $attach_output (i32.const 0) (i32.const 0) (i32.const 0))
            (call $store_int (i32.const 0) (i32.const 0)))
      (else (if
               (i32.eq (get_local $n) (i32.const 1))
               (then  (call $attach_output (i32.const 0) (i32.const 0) (i32.const 0))
                      (call $store_int (i32.const 0) (i32.const 1)))
               (else  (call $attach_output (i32.const 0) (i32.const 0) (i32.const 2))
                      (call $attach_output (i32.const 1) (i32.const 1) (i32.const 1))
                      (call $attach_output (i32.const 2) (i32.const 2) (i32.const 1))
                      (call $attach_output (i32.const 3) (i32.const 3) (i32.const 1))
                      (call $move_lazy_input (i32.const 1) (i32.const 0) (i32.const 1))
                      (call $move_lazy_input (i32.const 2) (i32.const 0) (i32.const 0))
                      (call $move_lazy_input (i32.const 3) (i32.const 0) (i32.const 0))
                      (call $attach_output_child (i32.const 2) (i32.const 1) (i32.const 4) (i32.const 1))
                      (call $attach_output_child (i32.const 4) (i32.const 0) (i32.const 4) (i32.const 0))
                      (call $store_int (i32.const 4) (i32.add (get_local $n) (i32.const -1)))
                      (call $attach_output_child (i32.const 3) (i32.const 1) (i32.const 4) (i32.const 1))
                      (call $attach_output_child (i32.const 4) (i32.const 0) (i32.const 4) (i32.const 0))
                      (call $store_int (i32.const 4) (i32.add (get_local $n) (i32.const -2)))
                      (call $attach_output_child (i32.const 1) (i32.const 1) (i32.const 4) (i32.const 1))
                      (call $attach_output_child (i32.const 4) (i32.const 0) (i32.const 5) (i32.const 2))
                      (call $set_encode (i32.const 5) (i32.const 2))
                      (call $add_path (i32.const 5) (i32.const 0))
                      (call $attach_output_child (i32.const 4) (i32.const 1) (i32.const 6) (i32.const 2))
                      (call $set_encode (i32.const 6) (i32.const 3))
                      (call $add_path (i32.const 6) (i32.const 0))
                      (call $set_encode (i32.const 0) (i32.const 1))
                      (call $add_path (i32.const 0) (i32.const 0)))))))
  (func $_start (type 3)
    i32.const 0
    i32.const 0
    call $attach_input
    i32.const 0
    i32.const 0
    call $get_int
    call $fib)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 2)
  (global (;0;) (mut i32) (i32.const 66576))
  (export "memory" (memory 0))
  (export "_start" (func $_start)))
