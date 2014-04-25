package test;

class Feline {
	public String type = "f ";
	public Feline() {
		System.out.print("feline ");
	}
}

public class Cougar extends Feline {
	public Cougar() {
	
		System.out.print("cougar "); 
	}
	
	public static void main(String[] args) {
		new Cougar().go();
	}
	
	void go() {
		type = "c ";
		System.out.print(this.type + super.type);
	}
}
