struct Path
{
	Ray ray{};
	Hit hit{};

	rtm::vec3 throughput{1.0f};
	rtm::vec3 radiance  {0.0f};
};
