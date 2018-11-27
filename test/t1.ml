
let _ =
  Fiber.create print_string "hello, world\n" |> Fiber.resume
