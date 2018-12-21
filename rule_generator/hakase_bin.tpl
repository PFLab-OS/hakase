-include {% for file in depends | map('regex_replace','.cc','.d') | select('string') | list %} {{ file }}{% endfor %}

{% set depends_var = "DEPENDS_" + target | regex_replace('/', '_') | regex_replace('\.', '_')  | upper() -%}
{{ depends_var }}:={% for file in depends %} {{ file }}{% endfor %}
{{ target }}:$({{ depends_var }})
	@echo "CC: (docker)/{{ dir }} : {{ target }} <=$({{ depends_var }})"
	@$(DOCKER_CMD) $(BUILD_CONTAINER) g++ {{ cflags }} -o /workdir/{{ target }} $(addprefix /workdir/,$({{ depends_var }}))
