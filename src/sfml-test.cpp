/*
 * Matthew Todd Geiger <mgeiger@newtonlabs.com>
 * Apr 6, 2022
 * sfml-test.cpp
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define SFML_SYSTEM_LINUX

// Standard Library
#include <iostream>
#include <ctime>
#include <mutex>
#include <algorithm>
#include <errno.h>
#include <cstring>

// SFML Includes
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <SFML/Main.hpp>

// X Includes
#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>

#ifdef DEBUG
#define DPRINT(str, ...) printf("DEBUG --> " str "\n", ##__VA_ARGS__)
#else
#define DPRINT(str, ...)
#endif

#define EPRINT(str, ...) fprintf(stderr, "ERROR --> " str "\nSTERROR: %s\n", ##__VA_ARGS__, strerror(errno))

#define FATAL(str, ...) { fprintf(stderr, "FATAL --> " str "\nSTRERROR: %s\n", ##__VA_ARGS__, strerror(errno)); exit(EXIT_FAILURE); } 

#if defined(UNUSED_PARAMETER)
#error UNUSED_PARAMETER has already been defined!
#else
#define UNUSED_PARAMETER(x) (void)(x)
#endif

#ifndef BASE_DIR
#define BASE_DIR "./"
#endif

#define FONT_DIR "fonts"
#define FONTSIZE 13
#define CIRCLESIZE 10.0f
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define MOVEMENTSPEED 400.0f
#define VELOCITY 600.0
#define JUMPVELOCITY 1000.0
#define ACCELERATION 1200.0

sf::Mutex p_shapeMutex;
sf::CircleShape *p_shape = nullptr;
bool onTheGround = false;
bool jumpEvent = false;

typedef struct RenderThreadData {
	sf::RenderWindow *window;
	sf::Font *font;
} RenderThreadData;

void MoveCharacter(sf::CircleShape &character, double xOffset, double yOffset, sf::RenderWindow &window) {
	auto radius = character.getRadius();
	auto vec = character.getPosition();
	vec.x += xOffset;
	vec.y += yOffset;

	// Make sure the ball can never leave the screen
	vec.x = std::clamp<double>(vec.x, 0.0f, window.getSize().x-radius*2);
	vec.y = std::clamp<double>(vec.y, 0.0f, window.getSize().y-radius*2);	

	character.setPosition(vec);
}

void HandleKbdEvents(sf::RenderWindow &window, float deltaTime, bool inFocus) {
	// If the window is not in focus exit...
	if(!inFocus)
		return;	
	
	// Close window on escape keypress
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape)) {
		window.close();
		return;
	}

	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Space) && onTheGround)
		jumpEvent = true;
	else
		jumpEvent = false;

	// Handle ball control
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
		p_shapeMutex.lock();
		MoveCharacter(*p_shape, .0f-(MOVEMENTSPEED*deltaTime), 0.0f, window);
		p_shapeMutex.unlock();
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
		p_shapeMutex.lock();
		MoveCharacter(*p_shape, (MOVEMENTSPEED*deltaTime), 0.0f, window);
		p_shapeMutex.unlock();
	}
}

/*
 * Make sure that the window is actually set to active
 */
inline void WaitUntilActive(sf::RenderWindow &window) {
	while(!window.setActive(true));
}

/*
 * Handle all rendering related tasks
 */
void Render(RenderThreadData *threadData) {
	sf::RenderWindow *window = threadData->window;
	sf::Font *font = threadData->font;

	// Reactivate OpenGl Context
	window->setActive(true);

	// Create window, circle, and text objects
	sf::CircleShape shape(CIRCLESIZE);
	sf::Text text(std::string(""), *font, FONTSIZE);
	
	// Set shape to center of screen/set the color
	auto shapeX = (static_cast<float>(window->getSize().x)/2) - shape.getRadius();
	auto shapeY = (static_cast<float>(window->getSize().y)/2) - shape.getRadius();

	shape.setFillColor(sf::Color::Green);
	shape.setPosition(shapeX, shapeY);

	// Set text location
	text.setPosition(5, 5);

	// Create clock
	sf::Clock clock;

	// Get public handle on shape
	p_shapeMutex.lock();
	p_shape = &shape;
	p_shapeMutex.unlock();

	while(true) {
		auto elapsedTime = clock.restart().asSeconds();
		auto fps = 1.0f/elapsedTime; // Calculate FPS
		text.setString(std::string("Renderer FPS: ") + std::to_string(fps)); // Convert FPS to string

		if(window->isOpen()) {
			window->clear();
			window->draw(shape); // Draw circle
			window->draw(text); // Draw text 
			window->display(); // Push buffer to display
		} else {
			break;
		}
	}

	DPRINT("Thread exiting safetly");
	return;
}

sf::Vector2i PrimaryMonitorCoordinates() {
	// Create xcb handle
	auto conn = xcb_connect(nullptr, nullptr);

	if(xcb_connection_has_error(conn))
		FATAL("Failed to get XCB handle");

	// Get root screen
	const auto setup = xcb_get_setup(conn);
	auto screen = xcb_setup_roots_iterator(setup).data;
	auto mon = xcb_randr_get_output_primary(conn, screen->root);
	auto reply = xcb_randr_get_output_primary_reply(conn, mon, nullptr);

	if(reply == nullptr) {
		EPRINT("Could not recieve RANDR outputs");
		return {0, 0};
	}

	// Get root output device
	auto output = xcb_randr_get_output_info_reply(conn, xcb_randr_get_output_info(conn, reply->output, XCB_CURRENT_TIME), nullptr);
	if(output == nullptr) {
		EPRINT("Failed to get output handle!");
		return {0, 0};
	}

	if(output->crtc == XCB_NONE || output->connection == XCB_RANDR_CONNECTION_DISCONNECTED) {
		EPRINT("Display detected as disconnected or missing");
		return {0, 0};
	}

	// get root info
	auto crtc = xcb_randr_get_crtc_info_reply(conn, xcb_randr_get_crtc_info(conn, output->crtc, XCB_CURRENT_TIME), nullptr);
	if(crtc == nullptr)
		FATAL("Couldn't get crtc");

	sf::Vector2i ret = {crtc->x, crtc->y};

	free(crtc);
	free(output);
	free(reply);

	xcb_disconnect(conn);

	return ret;
}

void SetDefaultWindowPosition(sf::RenderWindow &window) {
	auto desktop = sf::VideoMode::getDesktopMode();
	auto windowDimensions = window.getSize();

	// xcb get monitor coordinates
	auto primaryPosition = PrimaryMonitorCoordinates();

	//sf::Vector2i primaryPosition = {0, 0};

	DPRINT("Desktop Width: %u\nDesktop Height: %u", desktop.width, desktop.height); 
	DPRINT("Default Window Position: (%u, %u)", window.getPosition().x, window.getPosition().y);
	DPRINT("Primary Monitor Position: (%u, %u)", primaryPosition.x, primaryPosition.y);

	sf::Vector2i windowPosition = {primaryPosition.x + static_cast<int>(desktop.width/2 - windowDimensions.x/2),
			primaryPosition.y + static_cast<int>(desktop.height/2 - windowDimensions.y/2)};

	window.setPosition(windowPosition);
}

sf::Vector2i GetWindowOffset(sf::Vector2i &lastPos, sf::RenderWindow &window) {
	auto newPosition = window.getPosition();
	sf::Vector2i offset = {newPosition.x - lastPos.x, newPosition.y - lastPos.y};

	lastPos = newPosition;

	return offset;
}

int main(int argc, char** argv) {
	UNUSED_PARAMETER(argc);

	bool inFocus = true;

	// Alert X that this will be a multithreaded application
	if(XInitThreads() == 0) {
		FATAL("Failed to init X Multithreaded");
	}

	// Load font for text
	sf::Font font;
	if(!font.loadFromFile(BASE_DIR FONT_DIR "/Ubuntu-Regular.ttf")) {
		std::cerr << "Failed to load fonts" << std::endl;
		return EXIT_FAILURE;	
	}

	// Create default context settings with an antialiasing level of 8
	sf::ContextSettings contextSettings(0, 0, 8);

	// Create window, circle, and text objects
	sf::RenderWindow window(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), argv[0],
		   	sf::Style::Titlebar | sf::Style::Close, contextSettings); // Disable resize for now
	
	window.setVerticalSyncEnabled(true);
	//window.setFramerateLimit(250);
	SetDefaultWindowPosition(window);
	window.setActive(false); // Disable OpenGl context before passing context to thread

	// Create data that will be passed to rendering thread
	RenderThreadData threadData = {&window, &font};

	// Create and launch thread for rendering
	sf::Thread thread(Render, &threadData);
	thread.launch();

	// Set clock
	sf::Clock clock;

	bool skipFirst = true;
	p_shapeMutex.lock();
	auto curWindowPosition = window.getPosition();
	p_shapeMutex.unlock();
	double vel = VELOCITY;

	while(true) {
		p_shapeMutex.lock();
		if(p_shape != nullptr) {
			p_shapeMutex.unlock();
			break;
		}
		p_shapeMutex.unlock();
	}

	// main loop
	while (true) {
		if(!window.isOpen()) {
			break;
		}

		auto elapsedTime = clock.restart().asSeconds();

		auto windowPositionOffset = GetWindowOffset(curWindowPosition, window);	

		if(windowPositionOffset.x != 0 || windowPositionOffset.y != 0) {
			DPRINT("Window Offset: (%d, %d)", windowPositionOffset.x, windowPositionOffset.y);
			
			if(skipFirst == true) {
				skipFirst = false;
				continue;
			}

			// TODO: Calculate gravity offsets caused by screen movement 
		}

		// Handle jump event
		if(jumpEvent) {
			jumpEvent = false;
			vel = 0.0-JUMPVELOCITY;
		}

		p_shapeMutex.lock();
		MoveCharacter(*p_shape, 0.0, vel*elapsedTime, window);
		if(p_shape->getPosition().y == window.getSize().y-p_shape->getRadius()*2)
			onTheGround = true;
		else
			onTheGround = false;
		p_shapeMutex.unlock();

		if(vel != 0.0)
			vel += ACCELERATION*elapsedTime;

		// Calculate bounce velocity
		if(onTheGround == true && vel > 0.0) {
			vel = 0.0-(vel/2);
			if(vel > -60)
				vel = 0;
		}

		bool closed = false;

		// Handle events
		sf::Event event;
		while(window.pollEvent(event)) {
			switch(event.type) {
				case sf::Event::Closed:
					window.close();
					break;
				case sf::Event::Resized: // TODO: Add screen resize handler
					break;
				case sf::Event::GainedFocus:
					inFocus = true;
					break;
				case sf::Event::LostFocus:
					inFocus = false;
					break;				
				default:
					break;
			}

			if(closed)
				break;
		}

		HandleKbdEvents(window, elapsedTime, inFocus);
	}

	// Wait for renderer to finish
	DPRINT("Waiting for rendering thread");
	thread.terminate();

	DPRINT("Shutdown Finished");

	_Exit(EXIT_SUCCESS);
}
