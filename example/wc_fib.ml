let is_space = function
    ' ' | '\012' | '\n' | '\r' |'\t' -> true
    | _ -> false

let read_char = Chan.read_char
let wc chan =
  let c = ref 0 in
  let w = ref 0 in
  let l = ref 0 in
  let prev_is_space = ref true in
  try
    while true do
      let char = read_char chan in
      let char_is_space = is_space char in
      if not !prev_is_space && char_is_space then
        incr w;
      if char = '\n' then
        incr l;
      incr c;
      prev_is_space := char_is_space
    done
  with
    End_of_file -> Printf.printf "% 7i% 7i% 7i\n" !l !w !c;
                   Fiber.break ()

let _ =
  Fiber.create wc (Chan.make Unix.stdin) |> ignore;
  Fiber.run ()
