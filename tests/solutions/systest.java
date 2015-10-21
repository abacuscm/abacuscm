import java.util.*;
import java.io.*;
import java.net.*;

public class systest
{
	private static void testWriteFile()
	{
		try
		{
			File target = new File("foo.txt");
			try (FileWriter out = new FileWriter(target))
			{
			}
			System.out.println("Created foo.txt (bad)");
			target.delete();
		}
		catch (SecurityException e)
		{
			System.out.println("SecurityException writing foo.txt (good): " + e.getMessage());
		}
		catch (IOException e)
		{
			System.out.println("Failed to created foo.txt (okay): " + e.getMessage());
		}
	}

	private static void testExec()
	{
		try
		{
			Process p = new ProcessBuilder("/bin/true").start();
			System.out.println("Successfully started /bin/true (bad)");
			p.waitFor();
		}
		catch (SecurityException e)
		{
			System.out.println("SecurityException running /bin/true (good): " + e.getMessage());
		}
		catch (IOException e)
		{
			System.out.println("IOException running /bin/true (might just be missing): " + e.getMessage());
		}
		catch (InterruptedException e)
		{
			System.out.println("InterruptedException: should not happen!");
		}
	}

	private static void testSocket()
	{
		try(Socket sock = new Socket(InetAddress.getLocalHost(), 22))
		{
			System.out.println("Connected to localhost:22 (bad)");
		}
		catch (SecurityException e)
		{
			System.out.println("SecurityException connecting to localhost:22 (good): " + e.getMessage());
		}
		catch (IOException e)
		{
			System.out.println("IOException connecting to localhost:22 (might just be not listening): " + e.getMessage());
		}
	}

	public static void main(String[] args)
	{
		testWriteFile();
		testExec();
		testSocket();
	}
}
