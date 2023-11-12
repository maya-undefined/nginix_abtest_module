# nginix_abtest_module

ABTesting module for Nginix

One possible AB testing system would be to load a large number of abtests to run experiments against. 

Experiments could be described as

```
buckets:
  -optionA: /option_A
  -optionB: /option_B
  -optionC: /option_C
experiment_chance: 5
default: /
```

The `experiment_chance` controls how often we experiment or not. 
