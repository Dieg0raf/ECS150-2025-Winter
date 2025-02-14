Table table;

condition_variable canSmoke, tableEmpty;

void agent() {
  // lock()
  while (true) {
    items = ["papers", "lighter", "tobacco"];
    
    lock()

    // wait while there are 3 items on the table (meaning a smoker is smoking)
    while(!table.isEmpty()) {
      canSmoke.wait(finsihedSmoking);
    }

    table.add(items.randomPop());
    table.add(items.randomPop());

    broadcast(canSmoke); // broadcasts to smokers that they can smoke
    unlock()

  }

  // unlock()
}

void smoker(string item) {
  // lock()
  while (true) {
    
    lock()
    
    // while table has the item, then he already has the item, but needs the other items to smoke, so thread waits
    while (table.has(item)) {
      canSmoke.wait(canSmoke);
    }

    table.add(item);
    items = table.getAllItems();

    unlock()
    smoke(items);
    lock()

    signal(tableEmpty); // signals the agent that he is done smoking

    unlock()
  }
  // unlock()
}
