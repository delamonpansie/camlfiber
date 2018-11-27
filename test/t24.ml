module S = Set.Make(String)

let n = 100
let main () =
  let zombies = Array.init 4096 (fun _ -> Fiber.create Fiber.yield ()) in
  Array.iter Fiber.resume zombies;
  Array.iter Fiber.resume zombies;
  Gc.compact ()


let _ = Fiber.run main ()
