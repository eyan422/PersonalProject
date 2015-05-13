package movable;

interface Movable1 {
	void move();
}

class Person implements Movable1 {
	public void move() { System.out.println("Person move"); }
}

class Vehicle implements Movable1 {
	public void move() { System.out.println("Vehicle move"); }
}

class Movable {
	// INSERT CODE HERE
	void walk(Movable1 movable) 
	{
		movable.move();
	}
	
	public static void main(String args[])
	{
		Movable m = new Movable();
		Person p = new Person();
		Vehicle v = new Vehicle();
		
		m.walk(p);
		m.walk(v);
	}
}