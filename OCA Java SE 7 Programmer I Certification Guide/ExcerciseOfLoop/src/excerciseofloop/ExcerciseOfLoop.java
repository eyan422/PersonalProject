package excerciseofloop;

public class ExcerciseOfLoop {
	public static void main(String args[])
	{
		int i = 10;
		
		/*
		do
			while (i < 15)
				i = i + 20;
		while (i < 2);
		System.out.println(i);
		*/
		
		do
			while (i++ < 15)
			i = i + 20;
		while (i < 2);
		System.out.println(i);
		
		int a = 10;
		if (a++ > 10) {
			System.out.println("true");
		} 
		{
		System.out.println("false");
		}
		System.out.println("ABC");
		
		/*
		int num = 20;
		final int num2;
		num2 = 20;
		switch (num) {
			default: System.out.println("default");
			case num2: System.out.println(4);
			break;
		}
		
		int num = 120;
		switch (num) {
			default: System.out.println("default");
			case 0: System.out.println("case1");
			case 10*2-20: System.out.println("case2");
			break;
		}
		*/
		
		byte foo = 120;
		switch (foo) {
			default: System.out.println("ejavaguru"); break;
			case 2: System.out.println("e"); break;
			case 120: System.out.println("ejava");
			case 121: System.out.println("enum");
			case 127: System.out.println("guru"); break;
		}
		
		boolean myVal = false;
		if (myVal=true)
			for (int m = 0; m < 2; m++) System.out.println(m);
		else System.out.println("else");
	}
}
