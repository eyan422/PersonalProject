package thispointer;

public class This {
	String name;
	String address;
	
	This() 
	{
		name = "NoName";
		address = "NoAddress";
	}
	
	This(String name, String address) 
	{
		this();
		
		if (name != null) 
			this.name = name;
		
		if (address != null) 
			this.address = address;
	}
	
	public static void main(String args[])
	{
		
	}
}
