package test;

public class Sequencer { 
	
	Sequencer() 
	{ 
		System.out.print("c "); 
	}
	
	{ 
		System.out.print("y "); 
	}
	
	public static void main(String[] args) 
	{ 
		new Sequencer().go(); 
	} 
	
	void go() { System.out.print("g "); }
	
	static { System.out.print("x "); } 
} 


