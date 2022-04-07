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

// Standard Library
#include <iostream>
#include <ctime>
#include <mutex>

// SFML Includes
#include <SFML/Graphics.hpp>

// X Includes
#include <X11/Xlib.h>

#ifdef DEBUG
#define DPRINT(str, ...) printf("DEBUG --> " str "\n", ##__VA_ARGS__)
#else
#define DPRINT(str, ...)
#endif

#if defined(UNUSED_PARAMETER)
#error UNUSED_PARAMETER has already been defined!
#else
#define UNUSED_PARAMETER(x) (void)(x)
#endif

#define FONT_DIR "fonts"
#define FONTSIZE 13
#define CIRCLESIZE 10.0f
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define MOVEMENTSPEED 160.0f

std::mutex p_shapeMutex;
sf::CircleShape *p_shape = nullptr;

typedef struct RenderThreadData {
	sf::RenderWindow *window;
	sf::Font *font;
} RenderThreadData;

void HandleKbdEvents(sf::Window &window, float deltaTime, bool inFocus) {
	if(!inFocus)
		return;	
	
	// Close window on escape keypress
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Escape))
		window.close();
	
	// Handle ball control
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
		p_shapeMutex.lock();
		p_shape->setPosition(p_shape->getPosition().x, p_shape->getPosition().y-(MOVEMENTSPEED*deltaTime));
		p_shapeMutex.unlock();
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
		p_shapeMutex.lock();
		p_shape->setPosition(p_shape->getPosition().x, p_shape->getPosition().y+(MOVEMENTSPEED*deltaTime));
		p_shapeMutex.unlock();
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
		p_shapeMutex.lock();
		p_shape->setPosition(p_shape->getPosition().x-(MOVEMENTSPEED*deltaTime), p_shape->getPosition().y);
		p_shapeMutex.unlock();
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
		p_shapeMutex.lock();
		p_shape->setPosition(p_shape->getPosition().x+(MOVEMENTSPEED*deltaTime), p_shape->getPosition().y);
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
	WaitUntilActive(*window);

	// Create window, circle, and text objects
	sf::CircleShape shape(CIRCLESIZE);
	sf::Text text(std::string(""), *font, FONTSIZE);
	
	// Set shape to center of screen/set the color
	auto shapeX = (window->getSize().x/2) - shape.getRadius();
	auto shapeY = (window->getSize().y/2) - shape.getRadius();

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

	while(window->isOpen()) {
		auto elapsedTime = clock.restart().asSeconds();
		auto fps = 1.0f/elapsedTime; // Calculate FPS
		text.setString("Renderer FPS: " + std::to_string(fps)); // Convert FPS to string

		window->clear(); // Clear screen
		
		p_shapeMutex.lock();
		window->draw(shape); // Draw circle
		p_shapeMutex.unlock();

		window->draw(text); // Draw text 
		window->display(); // Push buffer to display
	}
}

int main(int argc, char** argv) {
	UNUSED_PARAMETER(argc);

	bool inFocus = true;

	// Alert X that this will be a multithreaded application
	XInitThreads();

	// Load font for text
	sf::Font font;
	if(!font.loadFromFile(FONT_DIR "/Ubuntu-Regular.ttf")) {
		std::cerr << "Failed to load fonts" << std::endl;
		return EXIT_FAILURE;	
	}

	// Create default context settings with an antialiasing level of 8
	sf::ContextSettings contextSettings(0, 0, 8);

	// Create window, circle, and text objects
	sf::RenderWindow window(sf::VideoMode(SCREEN_WIDTH, SCREEN_HEIGHT), argv[0], sf::Style::Titlebar | sf::Style::Close, contextSettings);
	window.setFramerateLimit(250);
	window.setActive(false); // Disable OpenGl context before passing context to thread

	// Create data that will be passed to rendering thread
	RenderThreadData threadData = {&window, &font};

	// Create and launch thread for rendering
	sf::Thread thread(Render, &threadData);
	thread.launch();

	// Set clock
	sf::Clock clock;

	// main loop
	while (window.isOpen()) {
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
		}

		auto elapsedTime = clock.restart().asSeconds();

		// Handle Keyboard input
		// TODO: Make this multi-threaded
		HandleKbdEvents(window, elapsedTime, inFocus);	
	}

	// Wait for renderer to finish
	thread.wait();

	return EXIT_SUCCESS;
}
