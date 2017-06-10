package org.eclipse.californium.examples;

import java.io.IOException;

import org.eclipse.californium.core.CoapServer;
import org.eclipse.californium.core.network.config.NetworkConfig;

import org.eclipse.californium.proxy.DirectProxyCoapResolver;
import org.eclipse.californium.proxy.ProxyHttpServer;
import org.eclipse.californium.proxy.resources.ForwardingResource;
import org.eclipse.californium.proxy.resources.ProxyCoapClientResource;
import org.eclipse.californium.proxy.resources.ProxyHttpClientResource;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

/**
 * Http2CoAP: Insert in browser:
 *     URI: http://localhost:8080/proxy/coap://localhost:PORT/target
 * 
 * CoAP2CoAP: Insert in Copper:
 *     URI: coap://localhost:PORT/coap2coap
 *     Proxy: coap://localhost:PORT/targetA
 *
 * CoAP2Http: Insert in Copper:
 *     URI: coap://localhost:PORT/coap2http
 *     Proxy: http://lantersoft.ch/robots.txt
 */
public class ExampleCrossProxy implements BundleActivator{
	
	
	
	private static final int PORT = NetworkConfig.getStandard().getInt(NetworkConfig.Keys.COAP_PORT);

	private CoapServer targetServerA;
	
	public ExampleCrossProxy() throws IOException {
		ForwardingResource coap2coap = new ProxyCoapClientResource("coap2coap");
		ForwardingResource coap2http = new ProxyHttpClientResource("coap2http");
		
		// Create CoAP Server on PORT with proxy resources form CoAP to CoAP and HTTP
		targetServerA = new CoapServer(PORT);
		targetServerA.add(coap2coap);
		targetServerA.add(coap2http);
		//targetServerA.add(new TargetResource("target"));
		targetServerA.start();
		
		ProxyHttpServer httpServer = new ProxyHttpServer(8080);
		httpServer.setProxyCoapResolver(new DirectProxyCoapResolver(coap2coap));
		
		System.out.println("CoAP resource \"target\" available over HTTP at: http://localhost:8080/proxy/coap://localhost:PORT/target");
	}
	
	@Override
	public void start(BundleContext context) throws Exception {
		//System.out.println("Hello World!!");
		new ExampleCrossProxy();
	}

	@Override
	public void stop(BundleContext context) throws Exception {
	      System.out.println("Goodbye PROXYYYYYYYYYYYYY!!");
	}
	//public static void main(String[] args) throws Exception {}
}
