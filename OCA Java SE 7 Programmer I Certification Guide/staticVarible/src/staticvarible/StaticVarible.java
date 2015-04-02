package staticvarible;

public class StaticVarible {
	
	
	static void print(){
		System.out.println(bankVault);
	}
	
	static double interest(double num1, double num2, double num3){
		return (num1+num2+num3)/3;
	}
	
	String name;
	static int bankVault;
}

class TestEmp{
	public static void main(String[] args){
		StaticVarible emp1 = new StaticVarible();
		StaticVarible emp2 = new StaticVarible();
		
		/*
		emp1.bankVault = 10;
		emp2.bankVault = 20;
		*/
		StaticVarible.bankVault = 10;
		StaticVarible.bankVault = 20;
		emp1.bankVault = 30;
		emp2.bankVault = 40;
		
		
		System.out.println(StaticVarible.bankVault);
		System.out.println(emp1.bankVault);
		System.out.println(emp2.bankVault);
		StaticVarible.print();
		System.out.println(StaticVarible.bankVault);
		
	}
}

