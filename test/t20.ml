let main () =
  let v = Fiber.MVar.create_empty () in
  assert(Fiber.MVar.take_available v = None);
  Fiber.MVar.put v "ok";
  assert(Fiber.MVar.take_available v = Some "ok")

let _ = Fiber.run main ()
