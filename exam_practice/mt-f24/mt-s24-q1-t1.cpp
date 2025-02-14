// assuming both operations aren't atomic
//
// no locks in use
// race condition flip_switch() (toggle the same light)


thread 1
// checks if living room_is_dark() (1)
//
// sees that the light is off (4)
//
//
// turns off the light (7)



