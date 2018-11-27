let main () =
  let c = Fiber.Condition.create () in
  Fiber.Condition.signal c

let _ = Fiber.run main ()
