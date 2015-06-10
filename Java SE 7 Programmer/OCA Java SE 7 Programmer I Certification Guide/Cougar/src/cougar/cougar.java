package cougar;

class Feline {
	public String type = "f ";
	
	public Feline() {
		System.out.print("feline ");
	}
}

public class cougar extends Feline {
	public cougar() {
		System.out.print("cougar "); 
	}
	
	public static void main(String[] args) {
		new cougar().go();
	}
	
	void go() {
		type = "c ";
		System.out.print(this.type + super.type);
	}
}
