package workshop;

class Laptop {
	String memory = "1GB";
}

class Workshop {
	public static void main(String args[]) {
		
		Laptop life = new Laptop();
		
		repair(life);
		
		System.out.println(life.memory);
	}
	
	public static void repair(Laptop laptop) {
		laptop = new Laptop();
		laptop.memory = "2GB";
	}
}
