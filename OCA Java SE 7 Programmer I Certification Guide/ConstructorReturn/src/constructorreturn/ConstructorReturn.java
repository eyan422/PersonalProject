package constructorreturn;

class test {
	//void ConstructorReturn()
	test()
	{
		System.out.println("test with return statement");
		return;
	}
}

public class ConstructorReturn{
	public static void main(String ... args)
	{
		test t = new test();
	}
}
